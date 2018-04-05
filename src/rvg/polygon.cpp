// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/polygon.hpp>
#include <rvg/context.hpp>
#include <katachi/stroke.hpp>
#include <vpp/vk.hpp>
#include <nytl/vecOps.hpp>
#include <dlg/dlg.hpp>

// TODO: we currently assume that update is always called before
// updateDevice but what if e.g. updateDevice was triggered by
// a disable call?

namespace rvg {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

} // anon namespace

// Polygon
Polygon::Polygon(Context& ctx) : DeviceObject(ctx) {
}

void Polygon::update(Span<const Vec2f> points, const DrawMode& mode) {
	constexpr auto fringe = 1.f; // TODO

	fill_.points.clear();
	fill_.color.clear();
	fill_.aa.points.clear();
	fill_.aa.color.clear();
	fill_.aa.aa.clear();
	stroke_.points.clear();
	stroke_.color.clear();
	stroke_.aa.clear();

	flags_.fill = mode.fill;
	if(flags_.fill) {
		if(mode.color.fill != flags_.colorFill) {
			flags_.colorFill = mode.color.fill;
			context().rerecord();
		}

		if(mode.aaFill != flags_.aaFill) {
			flags_.aaFill = mode.aaFill;
			context().rerecord();
		}

		if(flags_.aaFill) {
			dlg_assert(context().settings().aaStroke);
			auto fillHandler = [&](const auto& vertex) {
				fill_.points.push_back(vertex.position);
				if(mode.color.fill) {
					fill_.color.push_back(vertex.color);
				}
			};

			auto strokeHandler = [&](const auto& vertex) {
				fill_.aa.points.push_back(vertex.position);
				fill_.aa.aa.push_back(vertex.aa);
				if(mode.color.fill) {
					fill_.aa.color.push_back(vertex.color);
				}
			};

			constexpr auto fringe = 0.5f; // TODO
			if(mode.color.fill) {
				ktc::bakeColoredFillAA(points, mode.color.points, fringe,
					fillHandler, strokeHandler);
			} else {
				ktc::bakeFillAA(points, fringe, fillHandler, strokeHandler);
			}
		} else {
			fill_.points.insert(fill_.points.end(), points.begin(),
				points.end());
			if(mode.color.fill) {
				dlg_assert(mode.color.points.size() == points.size());
				fill_.color.insert(fill_.color.end(),
					mode.color.points.begin(), mode.color.points.end());
			}
		}
	}

	flags_.stroke = mode.stroke > 0.f;
	if(flags_.stroke) {
		if(mode.color.stroke != flags_.colorStroke) {
			flags_.colorStroke = mode.color.stroke;
			context().rerecord();
		}

		auto settings = ktc::StrokeSettings {mode.stroke};
		auto vertHandler = [&](const auto& vertex) {
			stroke_.points.push_back(vertex.position);
			if(context().settings().aaStroke) {
				stroke_.aa.push_back(vertex.aa);
			}

			if(mode.color.stroke) {
				stroke_.color.push_back(vertex.color);
			}
		};

		if(context().settings().aaStroke) {
			stroke_.mult = (mode.stroke * 0.5f + fringe * 0.5f) / fringe;
			settings.width += fringe * 0.5f;
		}

		if(mode.color.stroke) {
			ktc::bakeColoredStroke(points, mode.color.points, settings,
				vertHandler);
		} else {
			ktc::bakeStroke(points, settings, vertHandler);
		}

		if(context().settings().aaStroke && !stroke_.aaDs) {
			stroke_.aaDs = {context().dsLayoutStrokeAA(), context().dsPool()};
		}
	}

	context().registerUpdateDevice(this);
}

void Polygon::disable(bool disable, DrawType type) {
	if(type == DrawType::strokeFill || type == DrawType::fill) {
		flags_.disableFill = disable;
	}

	if(type == DrawType::strokeFill || type == DrawType::stroke) {
		flags_.disableStroke = disable;
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

bool Polygon::upload(Span<const Vec2f> points, Span<const Vec4u8> color,
		bool disable, bool colorFlag, vpp::BufferRange& pbuf,
		vpp::BufferRange& cbuf) {

	auto rerecord = false;
	auto pneeded = sizeof(vk::DrawIndirectCommand);
	pneeded += !disable * (sizeof(points[0]) * points.size());
	if(pbuf.size() < pneeded) {
		pneeded *= 2;
		pbuf = context().device().bufferAllocator().alloc(true,
			pneeded, vk::BufferUsageBits::vertexBuffer, 4u);
		rerecord = true;
	}

	auto pmap = pbuf.memoryMap();
	auto ptr = pmap.ptr();

	vk::DrawIndirectCommand cmd {};
	cmd.vertexCount = !disable * points.size();
	cmd.instanceCount = 1;
	write(ptr, cmd);

	if(disable) {
		return rerecord;
	}

	std::memcpy(ptr, points.data(), points.size() * sizeof(points[0]));

	// color
	if(!colorFlag) {
		return rerecord;
	}

	auto cneeded = (sizeof(color[0])) * color.size();
	if(cbuf.size() < cneeded) {
		cbuf = context().device().bufferAllocator().alloc(true,
			cneeded * 2 + 1, vk::BufferUsageBits::vertexBuffer, 4u);
		rerecord = true;
	}

	auto cmap = cbuf.memoryMap();
	std::memcpy(cmap.ptr(), color.data(), cneeded);

	return rerecord;
}

bool Polygon::updateDevice() {
	dlg_assert(valid());

	bool rerecord = false;

	if(flags_.fill) {
		rerecord |= upload(fill_.points, fill_.color,
			flags_.disableFill, flags_.colorFill,
			fill_.pBuf, fill_.cBuf);

		if(flags_.aaFill) {
			rerecord |= upload(fill_.aa.points, fill_.aa.color,
				flags_.disableFill, flags_.colorFill,
				fill_.aa.pBuf, fill_.aa.cBuf);

			auto needed = fill_.aa.aa.size() * sizeof(fill_.aa.aa[0]);
			if(fill_.aa.aaBuf.size() < needed) {
				auto& b = fill_.aa.aaBuf;
				auto usage = vk::BufferUsageBits::vertexBuffer;
				b = context().device().bufferAllocator().alloc(
					true, 2 * needed, usage, 4u);
				rerecord = true;
			}

			auto map = fill_.aa.aaBuf.memoryMap();
			std::memcpy(map.ptr(), fill_.aa.aa.data(),
				fill_.aa.aa.size() * sizeof(fill_.aa.aa[0]));
		}
	}

	if(flags_.stroke) {
		rerecord |= upload(stroke_.points, stroke_.color,
			flags_.disableStroke, flags_.colorStroke,
			stroke_.pBuf, stroke_.cBuf);

		// stroke anti aliasing
		if(context().settings().aaStroke) {
			auto needed = sizeof(float);
			needed += stroke_.aa.size() * sizeof(stroke_.aa[0]);
			if(stroke_.aaBuf.size() < needed) {
				auto& b = stroke_.aaBuf;
				auto usage = vk::BufferUsageBits::uniformBuffer |
					vk::BufferUsageBits::vertexBuffer;
				b = context().device().bufferAllocator().alloc(
					true, 2 * needed, usage, 4u);
				rerecord = true;

				vpp::DescriptorSetUpdate update(stroke_.aaDs);
				update.uniform({{b.buffer(), b.offset(), sizeof(float)}});
			}

			auto map = stroke_.aaBuf.memoryMap();
			auto ptr = map.ptr();
			write(ptr, stroke_.mult);
			std::memcpy(ptr, stroke_.aa.data(),
				stroke_.aa.size() * sizeof(stroke_.aa[0]));
		}
	}

	return rerecord;
}

void Polygon::fill(const DrawInstance& ini) const {
	dlg_assert(flags_.fill && valid() && fill_.pBuf.size() > 0);

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

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
		vk::cmdBindVertexBuffers(cmdb, 2, {c.buffer()}, {c.offset()});
	} else {
		vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off}); // dummy color
	}

	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);

	// aa stroke
	if(flags_.aaFill) {
		vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics,
			ctx.stripPipe());
		static constexpr auto type = uint32_t(2);
		vk::cmdPushConstants(cmdb, ctx.pipeLayout(),
			vk::ShaderStageBits::fragment, 0, 4, &type);

		auto& b = fill_.aa.pBuf;
		auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
		vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer()}, {off});

		dlg_assert(fill_.aa.aaBuf.size());
		auto& a = fill_.aa.aaBuf;
		vk::cmdBindVertexBuffers(cmdb, 1, {a.buffer()}, {a.offset()});
		vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
			ctx.pipeLayout(), Context::aaStrokeBindSet,
			{ctx.defaultStrokeAA()}, {});

		// color
		if(flags_.colorFill) {
			auto& c = fill_.aa.cBuf;
			vk::cmdBindVertexBuffers(cmdb, 2, {c.buffer()}, {c.offset()});
		} else {
			vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off}); // dummy
		}

		vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);
	}
}

void Polygon::stroke(const DrawInstance& ini) const {
	dlg_assert(flags_.stroke && valid() && stroke_.pBuf.size() > 0);

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());

	static constexpr auto type = uint32_t(2);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &type);

	// position and dummy uv buffer
	auto& b = stroke_.pBuf;
	auto off = b.offset() + sizeof(vk::DrawIndirectCommand);
	vk::cmdBindVertexBuffers(cmdb, 0, {b.buffer()}, {off});

	// aa
	if(context().settings().aaStroke) {
		dlg_assert(stroke_.aaBuf.size());
		auto& a = stroke_.aaBuf;
		vk::cmdBindVertexBuffers(cmdb, 1, {a.buffer()}, {a.offset() + 4});
		vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
			ctx.pipeLayout(), Context::aaStrokeBindSet,
			{stroke_.aaDs}, {});
	} else {
		vk::cmdBindVertexBuffers(cmdb, 1, {b.buffer()}, {off});
	}

	// color
	if(flags_.colorStroke) {
		auto& c = stroke_.cBuf;
		vk::cmdBindVertexBuffers(cmdb, 2, {c.buffer()}, {c.offset()});
	} else {
		vk::cmdBindVertexBuffers(cmdb, 2, {b.buffer()}, {off}); // dummy color
	}

	vk::cmdDrawIndirect(cmdb, b.buffer(), b.offset(), 1, 0);
}

} // namespace rvg
