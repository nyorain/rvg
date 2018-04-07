// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/polygon.hpp>
#include <rvg/context.hpp>
#include <katachi/stroke.hpp>
#include <vpp/vk.hpp>
#include <vpp/bufferOps.hpp>
#include <nytl/vecOps.hpp>
#include <dlg/dlg.hpp>
#include <optional>

// TODO: we currently assume that update is always called before
// updateDevice but what if e.g. updateDevice was triggered by
// a disable call?
// TODO: staging buffer uploadWork performance

namespace rvg {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

template<typename... Args> std::optional<vpp::UploadWork>
upload140(const vpp::BufferSpan& buf, Args&&... args) {
	// NOTE: we could also use direct writing for small updates
	dlg_assert(buf.valid());
	if(buf.buffer().mappable()) {
		vpp::writeMap140(buf, std::forward<Args>(args)...);
		return {};
	} else {
		return vpp::writeStaging140(buf, std::forward<Args>(args)...);
	}
}

} // anon namespace

// Polygon
Polygon::Polygon(Context& ctx) : DeviceObject(ctx) {
}

void Polygon::updateStroke(Span<const Vec2f> points, const DrawMode& mode) {
	if(mode.color.stroke != flags_.colorStroke) {
		flags_.colorStroke = mode.color.stroke;
		context().rerecord();
	}

	if(mode.aaStroke != flags_.aaStroke) {
		flags_.aaStroke = mode.aaStroke;
		context().rerecord();
	}

	dlg_assertm(!flags_.aaStroke || context().antiAliasing(),
		"Anti aliasing must be enabled in the context");

	auto sf = mode.aaStroke ? context().fringe() : 0.f;
	auto width = mode.stroke + sf;
	auto loop = mode.loop;
	if(points.size() > 2 && points.front() == points.back()) {
		loop = true;
		points = points.slice(0, points.size() - 1);
	}

	auto settings = ktc::StrokeSettings {width, loop, sf};
	auto vertHandler = [&](const auto& vertex) {
		stroke_.points.push_back(vertex.position);
		if(flags_.aaStroke) {
			stroke_.aa.push_back(vertex.aa);
		}

		if(flags_.colorStroke) {
			stroke_.color.push_back(vertex.color);
		}
	};

	if(flags_.aaStroke) {
		auto fringe = context().fringe();
		strokeMult_ = (mode.stroke * 0.5f + fringe * 0.5f) / fringe;
		settings.width += fringe * 0.5f;
	}

	if(mode.color.stroke) {
		ktc::bakeColoredStroke(points, mode.color.points, settings,
			vertHandler);
	} else {
		ktc::bakeStroke(points, settings, vertHandler);
	}

	if(flags_.aaStroke && !strokeDs_) {
		auto& layout = context().dsLayoutStrokeAA();
		strokeDs_ = {context().dsAllocator(), layout};
	}
}

void Polygon::updateFill(Span<const Vec2f> points, const DrawMode& mode) {
	if(mode.color.fill != flags_.colorFill) {
		flags_.colorFill = mode.color.fill;
		context().rerecord();
	}

	if(mode.aaFill != flags_.aaFill) {
		flags_.aaFill = mode.aaFill;
		context().rerecord();
	}

	if(flags_.aaFill) {
		dlg_assertm(context().antiAliasing(), "Anti aliasing must be \
			enabled in the context");

		// inset fill points and generate alpha blended stroke at
		// the edges for smoothness
		auto fillHandler = [&](const auto& vertex) {
			fill_.points.push_back(vertex.position);
			if(mode.color.fill) {
				fill_.color.push_back(vertex.color);
			}
		};

		auto strokeHandler = [&](const auto& vertex) {
			fillAA_.points.push_back(vertex.position);
			fillAA_.aa.push_back(vertex.aa);
			if(mode.color.fill) {
				fillAA_.color.push_back(vertex.color);
			}
		};

		if(mode.color.fill) {
			ktc::bakeColoredFillAA(points, mode.color.points,
				context().fringe(), fillHandler, strokeHandler);
		} else {
			ktc::bakeFillAA(points, context().fringe(), fillHandler,
				strokeHandler);
		}
	} else {
		// just copy color and points, no processing needed
		fill_.points.insert(fill_.points.end(), points.begin(), points.end());
		if(mode.color.fill) {
			dlg_assert(mode.color.points.size() == points.size());
			fill_.color.insert(fill_.color.end(),
				mode.color.points.begin(), mode.color.points.end());
		}
	}
}

void Polygon::update(Span<const Vec2f> points, const DrawMode& mode) {
	dlg_assertm(valid(), "Polygon must not be in invalid state");

	fill_.points.clear();
	fill_.color.clear();
	fillAA_.points.clear();
	fillAA_.color.clear();
	fillAA_.aa.clear();
	stroke_.points.clear();
	stroke_.color.clear();
	stroke_.aa.clear();

	if(mode.deviceLocal != flags_.deviceLocal) {
		flags_.deviceLocal = mode.deviceLocal;
		fill_ = {};
		fillAA_ = {};
		stroke_ = {};
	}

	flags_.fill = mode.fill;
	if(flags_.fill) {
		updateFill(points, mode);
	}

	flags_.stroke = mode.stroke > 0.f;
	if(flags_.stroke) {
		updateStroke(points, mode);
	}

	context().registerUpdateDevice(this);
}

void Polygon::disable(bool disable, DrawType type) {
	auto re = false;
	if(type == DrawType::strokeFill || type == DrawType::fill) {
		auto old = flags_.disableFill;
		re |= (old != (flags_.disableFill = disable));
	}

	if(type == DrawType::strokeFill || type == DrawType::stroke) {
		auto old = flags_.disableStroke;
		re |= (old != (flags_.disableStroke = disable));
	}

	context().registerUpdateDevice(this);
}

bool Polygon::disabled(DrawType type) const {
	bool ret = true;
	if(type == DrawType::strokeFill || type == DrawType::fill) {
		ret &= flags_.disableFill;
	}

	if(type == DrawType::strokeFill || type == DrawType::stroke) {
		ret &= flags_.disableStroke;
	}

	return ret;
}

bool Polygon::checkResize(vpp::SubBuffer& buf, vk::DeviceSize needed,
		vk::BufferUsageFlags usage) {
	needed = std::max(needed, vk::DeviceSize(16u));
	if(buf.size() < needed) {
		auto props = flags_.deviceLocal ?
			vk::MemoryPropertyBits::deviceLocal :
			vk::MemoryPropertyBits::hostVisible;
		auto memBits = context().device().memoryTypeBits(props);
		buf = {context().bufferAllocator(), needed * 2, usage, 4u, memBits};
		return true;
	}

	return false;
}

bool Polygon::upload(Draw& draw, bool disable, bool color) {
	auto rerecord = false;
	auto pneeded = sizeof(vk::DrawIndirectCommand);
	pneeded += !disable * (sizeof(draw.points[0]) * draw.points.size());
	rerecord |= checkResize(draw.pBuf, pneeded,
		vk::BufferUsageBits::vertexBuffer);

	vk::DrawIndirectCommand cmd {};
	cmd.vertexCount = !disable * draw.points.size();
	cmd.instanceCount = 1;

	if(disable) {
		upload140(draw.pBuf, vpp::raw(cmd));
		return rerecord;
	}

	auto points = vpp::raw(*draw.points.data(), draw.points.size());
	upload140(draw.pBuf, vpp::raw(cmd), points);

	// color
	if(!color) {
		return rerecord;
	}

	auto cneeded = (sizeof(draw.color[0])) * draw.color.size();
	rerecord |= checkResize(draw.cBuf, cneeded,
		vk::BufferUsageBits::vertexBuffer);
	upload140(draw.cBuf, vpp::raw(*draw.color.data(), draw.color.size()));

	return rerecord;
}

bool Polygon::upload(Stroke& stroke, bool disable, bool color, bool aa,
		float* mult) {

	bool rerecord = upload(static_cast<Draw&>(stroke), disable, color);
	if(aa) {
		auto needed = stroke.aa.size() * sizeof(stroke.aa[0]);
		vk::BufferUsageFlags usage = vk::BufferUsageBits::vertexBuffer;
		if(mult) {
			needed += sizeof(float);
			usage |= vk::BufferUsageBits::uniformBuffer;
		}

		rerecord |= checkResize(stroke.aaBuf, needed, usage);
		auto data = vpp::raw(*stroke.aa.data(), stroke.aa.size());

		if(mult) {
			upload140(stroke.aaBuf, *mult, data);
		} else {
			upload140(stroke.aaBuf, data);
		}
	}

	return rerecord;
}

bool Polygon::updateDevice() {
	dlg_assertm(valid(), "Polygon must not be in invalid state");

	bool rerecord = false;

	if(flags_.fill) {
		rerecord |= upload(fill_, flags_.disableFill, flags_.colorFill);
		if(flags_.aaFill) {
			rerecord |= upload(fillAA_, flags_.disableFill, flags_.colorFill,
				true, nullptr);
		}
	}

	if(flags_.stroke) {
		auto prev = stroke_.aaBuf.size();
		rerecord |= upload(stroke_, flags_.disableStroke, flags_.colorStroke,
			flags_.aaStroke, &strokeMult_);

		// check if buffer with our uniform was recreated
		if(prev != stroke_.aaBuf.size()) {
			auto& b = stroke_.aaBuf;
			vpp::DescriptorSetUpdate update(strokeDs_);
			update.uniform({{b.buffer(), b.offset(), sizeof(float)}});
		}
	}

	return rerecord;
}

void Polygon::fill(const DrawInstance& di) const {
	dlg_assertm(flags_.fill, "Polygon has no fill data");
	dlg_assertm(valid(), "Polygon must not be in an invalid state");
	dlg_assertm(&di.context == &context(),
		"Cannot stroke Polygon for another context");

	auto& ctx = di.context;
	auto& cmdb = di.cmdBuf;

	// fill
	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.fanPipe());

	static constexpr auto type = uint32_t(0);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &type);

	auto& b = fill_.pBuf;
	auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
	vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer()}, {off});
	vk::cmdBindVertexBuffers(cmdb, 1, {b.buffer()}, {off}); // dummy uv

	if(flags_.colorFill) {
		auto& c = fill_.cBuf;
		dlg_assert(c.size());
		vk::cmdBindVertexBuffers(cmdb, 2, {c.buffer()}, {c.offset()});
	} else {
		vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off}); // dummy color
	}

	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);

	// aa stroke
	if(flags_.aaFill) {
		stroke(di, fillAA_, true, flags_.colorFill, ctx.defaultStrokeAA(), 0u);
	}
}

void Polygon::stroke(const DrawInstance& di) const {
	dlg_assertm(flags_.stroke, "Polygon has no stroke data");
	dlg_assertm(valid(), "Polygon must not be in an invalid state");
	dlg_assertm(&di.context == &context(),
		"Cannot stroke Polygon for another context");

	stroke(di, stroke_, flags_.aaStroke, flags_.colorStroke, strokeDs_, 4u);
}

void Polygon::stroke(const DrawInstance& di, const Stroke& stroke, bool aa,
		bool color, vk::DescriptorSet aaDs, unsigned aaOff) const {

	dlg_assert(stroke.pBuf.size());

	auto& ctx = di.context;
	auto& cmdb = di.cmdBuf;
	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());

	// position and dummy uv buffer
	auto& b = stroke.pBuf;
	auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
	vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer()}, {off});

	// aa
	auto type = uint32_t(0);
	if(aa) {
		type = 2u;
		auto& a = stroke.aaBuf;
		dlg_assert(a.size());
		dlg_assert(aaDs);
		vk::cmdBindVertexBuffers(cmdb, 1, {a.buffer()}, {a.offset() + aaOff});
		vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
			ctx.pipeLayout(), Context::aaStrokeBindSet,
			{aaDs}, {});

	} else {
		vk::cmdBindVertexBuffers(cmdb, 1, {b.buffer()}, {off}); // dummy aa uv
	}

	// used to determine whether aa alpha blending is used
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(),
		vk::ShaderStageBits::fragment, 0, 4, &type);

	// color
	if(color) {
		auto& c = stroke.cBuf;
		dlg_assert(c.size());
		vk::cmdBindVertexBuffers(cmdb, 2, {c.buffer()}, {c.offset()});
	} else {
		vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off}); // dummy color
	}

	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);
}

} // namespace rvg
