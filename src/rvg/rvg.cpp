#include <rvg/context.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>
#include <rvg/polygon.hpp>
#include <rvg/shapes.hpp>
#include <rvg/state.hpp>
#include <rvg/deviceObject.hpp>

#include <katachi/path.hpp>
#include <katachi/stroke.hpp>
#include <katachi/curves.hpp>

#include <vpp/vk.hpp>
#include <vpp/formats.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/queue.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>
#include <nytl/vecOps.hpp>
#include <nytl/utf.hpp>
#include <cstring>

#include <rvg/nk_font/font.h>

#include <shaders/fill.vert.frag_scissor.h>
#include <shaders/fill.frag.frag_scissor.h>
#include <shaders/fill.vert.plane_scissor.h>
#include <shaders/fill.frag.plane_scissor.h>
#include <shaders/fill.frag.frag_scissor.edge_aa.h>
#include <shaders/fill.frag.plane_scissor.edge_aa.h>

// TODO(performance): optionally create (static) Polygons in deviceLocal memory.
// TODO(performance): cache points vec in *Shape::update
// TODO: something like default font(atlas) in context instead of dummy
//   texture?

namespace rvg {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

} // anon namespace

// Context
Context::Context(vpp::Device& dev, const ContextSettings& settings) :
	device_(dev), settings_(settings)
{
	constexpr auto sampleCount = vk::SampleCountBits::e1;

	// sampler
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::linear;
	samplerInfo.minFilter = vk::Filter::linear;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.25f;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::nearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.borderColor = vk::BorderColor::floatOpaqueWhite;
	texSampler_ = {dev, samplerInfo};

	samplerInfo.magFilter = vk::Filter::nearest;
	samplerInfo.minFilter = vk::Filter::nearest;
	fontSampler_ = {dev, samplerInfo};

	// layouts
	auto transformDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
	};

	auto paintDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::fragment),
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, -1, 1, &texSampler_.vkHandle()),
	};

	auto fontAtlasDSB = {
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, -1, 1, &fontSampler_.vkHandle()),
	};

	auto clipDistance = settings.clipDistanceEnable;
	auto scissorStage = clipDistance ?
		vk::ShaderStageBits::vertex :
		vk::ShaderStageBits::fragment;

	auto scissorDSB = {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer, scissorStage),
	};

	dsLayoutTransform_ = {dev, transformDSB};
	dsLayoutPaint_ = {dev, paintDSB};
	dsLayoutFontAtlas_ = {dev, fontAtlasDSB};
	dsLayoutScissor_ = {dev, scissorDSB};
	std::vector<vk::DescriptorSetLayout> layouts = {
		dsLayoutTransform_,
		dsLayoutPaint_,
		dsLayoutFontAtlas_,
		dsLayoutScissor_
	};

	if(settings.aaStroke) {
		auto aaStrokeDSB = {
			vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
				vk::ShaderStageBits::fragment),
		};

		dsLayoutStrokeAA_ = {dev, aaStrokeDSB};
		layouts.push_back(dsLayoutStrokeAA_);
	}

	pipeLayout_ = {dev, layouts, {
		{vk::ShaderStageBits::fragment, 0, 4}
	}};

	// pool
	vk::DescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = 500;
	poolSizes[0].type = vk::DescriptorType::uniformBuffer;

	poolSizes[1].descriptorCount = 500;
	poolSizes[1].type = vk::DescriptorType::combinedImageSampler;

	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo.maxSets = 100;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	dsPool_ = {dev, poolInfo};

	// pipeline
	using ShaderData = nytl::Span<const std::uint32_t>;
	auto vertData = ShaderData(fill_vert_frag_scissor_data);
	auto fragData = ShaderData(fill_frag_frag_scissor_data);

	if(clipDistance) {
		vertData = fill_vert_plane_scissor_data;
		if(settings.aaStroke) {
			fragData = fill_frag_plane_scissor_edge_aa_data;
		} else {
			fragData = fill_frag_plane_scissor_data;
		}
	} else if(settings.aaStroke) {
		fragData = fill_frag_frag_scissor_edge_aa_data;
	}

	auto fillVertex = vpp::ShaderModule(dev, vertData);
	auto fillFragment = vpp::ShaderModule(dev, fragData);

	vpp::ShaderProgram fillStages({
		{fillVertex, vk::ShaderStageBits::vertex},
		{fillFragment, vk::ShaderStageBits::fragment}
	});

	vk::GraphicsPipelineCreateInfo fanPipeInfo;

	fanPipeInfo.renderPass = settings.renderPass;
	fanPipeInfo.subpass = settings.subpass;
	fanPipeInfo.layout = pipeLayout_;
	fanPipeInfo.stageCount = fillStages.vkStageInfos().size();
	fanPipeInfo.pStages = fillStages.vkStageInfos().data();
	fanPipeInfo.flags = vk::PipelineCreateBits::allowDerivatives;

	// vertex attribs: vec2 pos, vec2 uv, vec4u8 color
	std::array<vk::VertexInputAttributeDescription, 3> vertexAttribs = {};
	vertexAttribs[0].format = vk::Format::r32g32Sfloat;

	vertexAttribs[1].format = vk::Format::r32g32Sfloat;
	vertexAttribs[1].location = 1;
	vertexAttribs[1].binding = 1;

	vertexAttribs[2].format = vk::Format::r8g8b8a8Unorm;
	vertexAttribs[2].location = 2;
	vertexAttribs[2].binding = 2;

	// position and uv are in different buffers
	// this allows polygons that don't use any uv-coords to simply
	// reuse the position buffer which will result in better performance
	// (due to caching) and waste less memory
	std::array<vk::VertexInputBindingDescription, 3> vertexBindings = {};
	vertexBindings[0].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[0].stride = sizeof(float) * 2; // position
	vertexBindings[0].binding = 0;

	vertexBindings[1].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[1].stride = sizeof(float) * 2; // uv
	vertexBindings[1].binding = 1;

	vertexBindings[2].inputRate = vk::VertexInputRate::vertex;
	vertexBindings[2].stride = sizeof(u8) * 4; // color
	vertexBindings[2].binding = 2;

	vk::PipelineVertexInputStateCreateInfo vertexInfo {};
	vertexInfo.pVertexAttributeDescriptions = vertexAttribs.data();
	vertexInfo.vertexAttributeDescriptionCount = vertexAttribs.size();
	vertexInfo.pVertexBindingDescriptions = vertexBindings.data();
	vertexInfo.vertexBindingDescriptionCount = vertexBindings.size();
	fanPipeInfo.pVertexInputState = &vertexInfo;

	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo.topology = vk::PrimitiveTopology::triangleFan;
	fanPipeInfo.pInputAssemblyState = &assemblyInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
	rasterizationInfo.polygonMode = vk::PolygonMode::fill;
	rasterizationInfo.cullMode = vk::CullModeBits::none;
	rasterizationInfo.frontFace = vk::FrontFace::counterClockwise;
	rasterizationInfo.depthClampEnable = false;
	rasterizationInfo.rasterizerDiscardEnable = false;
	rasterizationInfo.depthBiasEnable = false;
	rasterizationInfo.lineWidth = 1.f;
	fanPipeInfo.pRasterizationState = &rasterizationInfo;

	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo.rasterizationSamples = sampleCount;
	multisampleInfo.sampleShadingEnable = false;
	multisampleInfo.alphaToCoverageEnable = false;
	fanPipeInfo.pMultisampleState = &multisampleInfo;

	vk::PipelineColorBlendAttachmentState blendAttachment;
	blendAttachment.blendEnable = true;
	blendAttachment.alphaBlendOp = vk::BlendOp::add;
	blendAttachment.srcColorBlendFactor = vk::BlendFactor::srcAlpha;
	blendAttachment.dstColorBlendFactor = vk::BlendFactor::oneMinusSrcAlpha;
	blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::one;
	blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::zero;
	blendAttachment.colorWriteMask =
		vk::ColorComponentBits::r |
		vk::ColorComponentBits::g |
		vk::ColorComponentBits::b |
		vk::ColorComponentBits::a;

	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = 1;
	blendInfo.pAttachments = &blendAttachment;
	fanPipeInfo.pColorBlendState = &blendInfo;

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;
	fanPipeInfo.pViewportState = &viewportInfo;

	const auto dynStates = {
		vk::DynamicState::viewport,
		vk::DynamicState::scissor};

	vk::PipelineDynamicStateCreateInfo dynamicInfo;
	dynamicInfo.dynamicStateCount = dynStates.size();
	dynamicInfo.pDynamicStates = dynStates.begin();
	fanPipeInfo.pDynamicState = &dynamicInfo;

	// stripPipe
	auto stripPipeInfo = fanPipeInfo;
	stripPipeInfo.flags = vk::PipelineCreateBits::derivative;
	stripPipeInfo.basePipelineIndex = 0;

	auto stripAssemblyInfo = assemblyInfo;
	stripAssemblyInfo.topology = vk::PrimitiveTopology::triangleStrip;
	stripPipeInfo.pInputAssemblyState = &stripAssemblyInfo;

	auto pipes = vk::createGraphicsPipelines(dev, {},
		{fanPipeInfo, stripPipeInfo});
	fanPipe_ = {dev, pipes[0]};
	stripPipe_ = {dev, pipes[1]};

	// dummies
	constexpr std::uint8_t bytes[] = {0xFF, 0xFF, 0xFF, 0xFF};
	emptyImage_ = createTexture(dev, 1, 1,
		reinterpret_cast<const std::byte*>(bytes),
		TextureType::rgba32);

	dummyTex_ = {dsLayoutFontAtlas_, dsPool_};
	vpp::DescriptorSetUpdate update(dummyTex_);
	auto layout = vk::ImageLayout::general;
	update.imageSampler({{{}, emptyImage_.vkImageView(), layout}});

	identityTransform_ = {*this};
	pointColorPaint_ = {*this, ::rvg::pointColorPaint()};
	defaultScissor_ = {*this, Scissor::reset};
}

DrawInstance Context::record(vk::CommandBuffer cmdb) {
	DrawInstance ret { *this, cmdb };
	identityTransform_.bind(ret);
	defaultScissor_.bind(ret);
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		pipeLayout(), fontBindSet, {dummyTex_}, {});
	return ret;
}

bool Context::updateDevice() {
	auto visitor = [&](auto* obj) {
		dlg_assert(obj);
		return obj->updateDevice();
	};

	for(auto& ud : updateDevice_) {
		rerecord_ |= std::visit(visitor, ud);
	}

	updateDevice_.clear();
	auto ret = rerecord_;
	rerecord_ = false;
	return ret;
}

void Context::registerUpdateDevice(DeviceObject obj) {
	updateDevice_.insert(obj);
}

bool Context::deviceObjectDestroyed(::rvg::DeviceObject& obj) noexcept {
	auto it = updateDevice_.begin();
	auto visitor = [&](auto* ud) {
		if(&obj == ud) {
			updateDevice_.erase(it);
			return true;
		}

		return false;
	};

	for(; it != updateDevice_.end(); ++it) {
		if(std::visit(visitor, *it)) {
			return true;
		}
	}

	return false;
}

void Context::deviceObjectMoved(::rvg::DeviceObject& o,
		::rvg::DeviceObject& n) noexcept {

	auto it = updateDevice_.begin();
	auto visitor = [&](auto* ud) {
		if(&o == ud) {
			updateDevice_.erase(it);
			auto i = static_cast<decltype(ud)>(&n);
			updateDevice_.insert(i);
			return true;
		}

		return false;
	};

	for(; it != updateDevice_.end(); ++it) {
		if(std::visit(visitor, *it)) {
			return;
		}
	}
}

// Polygon
Polygon::Polygon(Context& ctx) : DeviceObject(ctx) {
}

void Polygon::update(Span<const Vec2f> points, const DrawMode& mode) {
	constexpr auto fringe = 2.f; // TODO

	fill_.points.clear();
	fill_.color.clear();
	stroke_.points.clear();
	stroke_.color.clear();

	flags_.fill = mode.fill;
	if(flags_.fill) {
		fill_.points.insert(fill_.points.end(), points.begin(), points.end());
		if(mode.color.fill) {
			dlg_assert(mode.color.points.size() == points.size());
			fill_.color.insert(fill_.color.end(),
				mode.color.points.begin(), mode.color.points.end());
		}
	}

	flags_.stroke = mode.stroke > 0.f;
	if(flags_.stroke) {
		auto settings = ktc::StrokeSettings {mode.stroke};
		auto baseHandler = [&](const auto& vertex) {
			stroke_.points.push_back(vertex.position);
			if(context().settings().aaStroke) {
				stroke_.aa.push_back(vertex.aa);
			}
		};

		if(context().settings().aaStroke) {
			stroke_.mult = (mode.stroke * 0.5f + fringe * 0.5f) / fringe;
			settings.width += fringe * 0.5f;
		}

		if(mode.color.stroke) {
			dlg_assert(mode.color.points.size() == points.size());
			auto handler = [&](const auto& vertex) {
				baseHandler(vertex);
				stroke_.color.push_back(vertex.color);
			};
			ktc::bakeColoredStroke(points, mode.color.points, settings,
				handler);
		} else {
			ktc::bakeStroke(points, settings, baseHandler);
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

// Buffer layouts (IDC = IndirectDrawCommand):
//
// -- start (align: 4)
// IDC
// -- pos: sizeof(IDC)
// position data (vec2f)
// -- pos: align(IDC + 2/3.f * (buf.size - sizeof(IDC)), 4u)
// color data [if any] (vec4u8)
// -- end

// TODO: we currently assume that update is always called before
// updateDevice but what if e.g. updateDevice was triggered by
// a disable call?

bool Polygon::updateDevice() {
	dlg_assert(valid());

	bool rerecord = false;
	auto upload = [&](auto& points, auto& color, auto disable,
			auto& colorFlag, auto& pbuf, auto& cbuf) {

		// pos
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
			return;
		}

		std::memcpy(ptr, points.data(), points.size() * sizeof(points[0]));

		// color
		if(colorFlag == color.empty()) {
			colorFlag ^= true;
			rerecord = true;
		}

		if(!colorFlag) {
			return;
		}

		auto cneeded = (sizeof(color[0])) * color.size();
		if(cbuf.size() < cneeded) {
			cbuf = context().device().bufferAllocator().alloc(true,
				cneeded * 2 + 1, vk::BufferUsageBits::vertexBuffer, 4u);
			rerecord = true;
		}

		auto cmap = cbuf.memoryMap();
		std::memcpy(cmap.ptr(), color.data(), cneeded);
	};

	if(flags_.fill) {
		auto f = flags_.colorFill;
		upload(fill_.points, fill_.color, flags_.disableFill, f,
			fill_.pBuf, fill_.cBuf);
		flags_.colorFill = f;
	}

	if(flags_.stroke) {
		auto f = flags_.colorStroke;
		upload(stroke_.points, stroke_.color, flags_.disableStroke, f,
			stroke_.pBuf, stroke_.cBuf);
		flags_.colorStroke = f;

		// stroke anti aliasing
		if(context().settings().aaStroke) {
			auto needed = sizeof(float);
			needed += stroke_.aa.size() * sizeof(stroke_.aa[0]);
			if(stroke_.aaBuf.size() < needed) {
				auto& b = stroke_.aaBuf;
				auto usage = vk::BufferUsageBits::uniformBuffer |
					vk::BufferUsageBits::vertexBuffer;
				b = context().device().bufferAllocator().alloc(
					true, 2 * needed, usage);
				rerecord = true;

				dlg_info("{}", b.buffer().vkHandle());
				dlg_info("{}", b.offset());
				dlg_info("{}", b.size());
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

// Shape
Shape::Shape(Context& ctx, std::vector<Vec2f> p, const DrawMode& d) :
		state_{std::move(p), std::move(d)}, polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

void Shape::update() {
	polygon_.update(state_.points, state_.drawMode);
}

void Shape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool Shape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

// Rect
RectShape::RectShape(Context& ctx, Vec2f p, Vec2f s,
	const DrawMode& d, std::array<float, 4> round) :
		state_{p, s, std::move(d), round}, polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

void RectShape::update() {
	if(state_.rounding == std::array<float, 4>{}) {
		auto points = {
			state_.position,
			state_.position + Vec {state_.size.x, 0.f},
			state_.position + state_.size,
			state_.position + Vec {0.f, state_.size.y},
			state_.position
		};
		polygon_.update(points, state_.drawMode);
	} else {
		constexpr auto steps = 12u;
		std::vector<Vec2f> points;

		auto& position = state_.position;
		auto& size = state_.size;
		auto& rounding = state_.rounding;

		// topRight
		if(rounding[0] != 0.f) {
			dlg_assert(rounding[0] > 0.f);
			points.push_back(position + Vec {0.f, rounding[0]});
			auto a1 = ktc::CenterArc {
				position + Vec{rounding[0], rounding[0]},
				{rounding[0], rounding[0]},
				nytl::constants::pi,
				nytl::constants::pi * 1.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position);
		}

		// topLeft
		if(rounding[1] != 0.f) {
			dlg_assert(rounding[1] > 0.f);
			auto x = position.x + size.x - rounding[1];
			points.push_back({x, position.y});
			auto a1 = ktc::CenterArc {
				{x, position.y + rounding[1]},
				{rounding[1], rounding[1]},
				nytl::constants::pi * 1.5f,
				nytl::constants::pi * 2.f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + Vec {size.x, 0.f});
		}

		// bottomRight
		if(rounding[2] != 0.f) {
			dlg_assert(rounding[2] > 0.f);
			auto y = position.y + size.y - rounding[2];
			points.push_back({position.x + size.x, y});
			auto a1 = ktc::CenterArc {
				{position.x + size.x - rounding[2], y},
				{rounding[2], rounding[2]},
				0.f,
				nytl::constants::pi * 0.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + size);
		}

		// bottomLeft
		if(rounding[3] != 0.f) {
			dlg_assert(rounding[3] > 0.f);
			points.push_back({position.x + rounding[3], position.y + size.y});
			auto y = position.y + size.y - rounding[3];
			auto a1 = ktc::CenterArc {
				{position.x + rounding[3], y},
				{rounding[3], rounding[3]},
				nytl::constants::pi * 0.5f,
				nytl::constants::pi * 1.f,
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + Vec {0.f, size.y});
		}

		// close it
		points.push_back(points[0]);
		polygon_.update(points, state_.drawMode);
	}
}

void RectShape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool RectShape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

// CircleShape
CircleShape::CircleShape(Context& ctx,
	Vec2f xcenter, Vec2f xradius, const DrawMode& xdraw,
	unsigned xpoints, float xstartAngle)
		: state_{xcenter, xradius, std::move(xdraw), xpoints, xstartAngle},
			polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

CircleShape::CircleShape(Context& ctx,
	Vec2f xcenter, float xradius, const DrawMode& xdraw,
	unsigned xpoints, float xstartAngle)
		: CircleShape(ctx, xcenter, {xradius, xradius},
			xdraw, xpoints, xstartAngle) {
}

void CircleShape::update() {
	dlg_assertl(dlg_level_warn, state_.pointCount > 2);

	std::vector<Vec2f> pts;
	pts.reserve(state_.pointCount);

	auto a = state_.startAngle;
	auto d = 2 * nytl::constants::pi / state_.pointCount;
	for(auto i = 0u; i < state_.pointCount + 1; ++i) {
		using namespace nytl::vec::cw::operators;
		auto p = Vec {std::cos(a), std::sin(a)} * state_.radius;
		pts.push_back(state_.center + p);
		a += d;
	}

	polygon_.update(pts, state_.drawMode);
}

void CircleShape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool CircleShape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

// FontAtlas
FontAtlas::FontAtlas(Context& ctx) {
	atlas_ = std::make_unique<nk_font_atlas>();
	nk_font_atlas_init_default(&nkAtlas());
	ds_ = {ctx.dsLayoutFontAtlas(), ctx.dsPool()};
}

FontAtlas::~FontAtlas() {
	nk_font_atlas_clear(&nkAtlas());
}

bool FontAtlas::bake(Context& ctx) {
	// TODO: use r8 format. Figure out why it did not work
	constexpr auto format = vk::Format::r8Uint;
	bool rerecord = false;

	int w, h;
	nk_font_atlas_begin(&nkAtlas());
	auto data = reinterpret_cast<const std::byte*>(
		nk_font_atlas_bake(&nkAtlas(), &w, &h, NK_FONT_ATLAS_ALPHA8));

	if(w == 0 || h == 0) {
		dlg_info("FontAtlas::bake on empty atlas");
		return false;
	}

	dlg_assert(w > 0 && h > 0);
	auto uw = unsigned(w), uh = unsigned(h);
	if(uw > width_ || uh > height_) {
		// TODO: allocate more than needed? 2d problem here though,
		//   the rectpack algorithm does not only produce squares
		texture_ = rvg::createTexture(ctx.device(), w, h,
			reinterpret_cast<const std::byte*>(data),
			rvg::TextureType::a8);
		rerecord = true;

		vpp::DescriptorSetUpdate update(ds_);
		update.imageSampler({{{}, texture_.vkImageView(),
			vk::ImageLayout::general}});
	} else {
		auto& qs = ctx.device().queueSubmitter();
		auto cmdBuf = ctx.device().commandAllocator().get(qs.queue().family());
		vk::beginCommandBuffer(cmdBuf, {});
		auto buf = vpp::fillStaging(cmdBuf, texture_.image(), format,
			vk::ImageLayout::general, {uw, uh, 1}, {data, uw * uh},
			{vk::ImageAspectBits::color});
		vk::endCommandBuffer(cmdBuf);

		// TODO: performance! Return the work/wait for submission
		qs.wait(qs.add({{}, {}, {}, 1, &cmdBuf.vkHandle(), {}, {}}));
	}

	nk_font_atlas_end(&nkAtlas(), {}, nullptr);
	return rerecord;
}

// Font
Font::Font(FontAtlas& atlas, const char* file, unsigned h) : atlas_(&atlas) {
	font_ = nk_font_atlas_add_from_file(&atlas.nkAtlas(), file, h, nullptr);
	if(!font_) {
		std::string err = "Could not load font ";
		err.append(file);
		throw std::runtime_error(err);
	}
}

Font::Font(FontAtlas& atlas, struct nk_font* font) :
	atlas_(&atlas), font_(font) {
}

float Font::width(std::string_view text) const {
	dlg_assert(font_);
	auto& handle = font_->handle;
	return handle.width(handle.userdata, handle.height, text.data(),
		text.size());
}

float Font::width(std::u32string_view text) const {
	dlg_assert(font_);
	float w = 0.f;
	for(auto c : text) {
		auto g = nk_font_find_glyph(font_, c);
		dlg_assert(g);
		w += g->xadvance;
	}

	return w;
}

float Font::height() const {
	dlg_assert(font_);
	return font_->handle.height;
}

// Text
constexpr auto vertIndex0 = 2;
constexpr auto vertIndex2 = 3;

Text::Text(Context& ctx, std::string_view xtext, const Font& f, Vec2f xpos) :
	Text(ctx, toUtf32(xtext), f, xpos) {
}

Text::Text(Context& ctx, std::u32string txt, const Font& f, Vec2f xpos) :
		DeviceObject(ctx), state_{std::move(txt), &f, xpos}, oldFont_(&f) {

	update();
	updateDevice();
}

void Text::update() {
	auto& font = state_.font;
	auto& text = state_.utf32;
	auto& position = state_.position;

	if(font != oldFont_) {
		if(&font->atlas() != &oldFont_->atlas()) {
			context().rerecord();
		}

		oldFont_ = font;
	}

	dlg_assert(font && font->nkFont());
	dlg_assert(posCache_.size() == uvCache_.size());

	posCache_.clear();
	uvCache_.clear();

	// good approximcation for usually-ascii
	posCache_.reserve(text.size());
	uvCache_.reserve(text.size());

	auto x = position.x;
	auto addVert = [&](const nk_font_glyph& glyph, unsigned i) {
		auto left = i == 0 || i == 3;
		auto top = i == 0 || i == 1;

		posCache_.push_back({
			x + (left ? glyph.x0 : glyph.x1),
			position.y + (top ? glyph.y0 : glyph.y1)});
		uvCache_.push_back({
			left ? glyph.u0 : glyph.u1,
			top ? glyph.v0 : glyph.v1});
	};

	for(auto c : text) {
		auto pglyph = nk_font_find_glyph(font->nkFont(), c);
		dlg_assert(pglyph);

		auto& glyph = *pglyph;

		// we render using a strip pipe. Those doubled points allow us to
		// jump to the next quad. Not less efficient than using a list pipe
		for(auto i : {1, 1, 0, 2, 3, 3}) {
			addVert(glyph, i);
		}

		x += glyph.xadvance;
	}

	context().registerUpdateDevice(this);
	dlg_assert(posCache_.size() == uvCache_.size());
}

bool Text::updateDevice() {
	bool rerecord = false;

	// now upload data to gpu
	dlg_assert(posCache_.size() == uvCache_.size());
	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto byteSize = sizeof(Vec2f) * posCache_.size();
	auto neededSize = ioff + 2 * byteSize;

	if(buf_.size() < neededSize) {
		neededSize *= 2;
		buf_ = context().device().bufferAllocator().alloc(true,
			neededSize, vk::BufferUsageBits::vertexBuffer);
		rerecord = true;
	}

	auto map = buf_.memoryMap();
	auto ptr = map.ptr();

	// the positionBuf contains the indirect draw command, if there is any
	vk::DrawIndirectCommand cmd {};
	cmd.vertexCount = !disable_ * posCache_.size();
	cmd.instanceCount = 1;
	write(ptr, cmd);

	std::memcpy(ptr, posCache_.data(), byteSize);
	ptr = ptr + (buf_.size() - ioff) / 2;
	std::memcpy(ptr, uvCache_.data(), byteSize);

	return rerecord;
}

void Text::draw(const DrawInstance& ini) const {
	if(posCache_.empty()) {
		return;
	}

	auto& ctx = ini.context;
	auto& cmdb = ini.cmdBuf;

	vk::cmdBindPipeline(cmdb, vk::PipelineBindPoint::graphics, ctx.stripPipe());
	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		ctx.pipeLayout(), Context::fontBindSet,
		{state_.font->atlas().ds()}, {});

	static constexpr auto type = uint32_t(1);
	vk::cmdPushConstants(cmdb, ctx.pipeLayout(), vk::ShaderStageBits::fragment,
		0, 4, &type);

	auto ioff = sizeof(vk::DrawIndirectCommand);
	auto off = buf_.offset() + ioff;

	// dummy color buffer
	auto vkbuf = buf_.buffer().vkHandle();
	vk::cmdBindVertexBuffers(cmdb, 0, {vkbuf, vkbuf, vkbuf},
		{off, off + (buf_.size() - ioff) / 2, off});
	vk::cmdDrawIndirect(cmdb, buf_.buffer(), buf_.offset(), 1, 0);
}

unsigned Text::charAt(float x) const {
	x += state_.position.x;
	for(auto i = 0u; i < posCache_.size(); i += 6) {
		auto end = posCache_[i + vertIndex2].x;
		if(x < end) {
			return i / 6;
		}
	}

	return unsigned(posCache_.size() / 6);
}

Rect2f Text::ithBounds(unsigned n) const {
	auto& text = utf32();
	auto& position = state_.position;

	if(posCache_.size() <= n * 6 || text.size() <= n) {
		throw std::out_of_range("Text::ithBounds");
	}

	// TODO: always use advance
	auto start = posCache_[n * 6 + vertIndex0];
	auto r = Rect2f {start - position, posCache_[n * 6 + vertIndex2] - start};

	if(r.size.x <= 0.f) {
		auto pglyph = nk_font_find_glyph(state_.font->nkFont(), text[n]);
		dlg_assert(pglyph);

		r.size.x = pglyph->xadvance;
	}

	return r;
}

void Text::State::utf8(std::string_view utf8) {
	utf32 = toUtf32(utf8);
}

std::string Text::State::utf8() const {
	return toUtf8(utf32);
}

float Text::width() const {
	if(utf32().empty()) {
		return 0.f;
	}

	auto first = ithBounds(0);
	auto last = ithBounds(utf32().length() - 1);
	return last.position.x + last.size.x - first.position.x;
}

bool Text::disable(bool disable) {
	auto ret = disable_;
	disable_ = disable;
	context().registerUpdateDevice(this);
	return ret;
}

// Transform
constexpr auto transformUboSize = sizeof(Mat4f);
Transform::Transform(Context& ctx) : Transform(ctx, identity<4, float>()) {
}

Transform::Transform(Context& ctx, const Mat4f& m) :
		DeviceObject(ctx), matrix_(m) {

	ubo_ = ctx.device().bufferAllocator().alloc(true, transformUboSize,
		vk::BufferUsageBits::uniformBuffer);
	ds_ = {ctx.dsLayoutTransform(), ctx.dsPool()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

bool Transform::updateDevice() {
	dlg_assert(valid() && ubo_.size() && ds_);
	auto map = ubo_.memoryMap();
	std::memcpy(map.ptr(), &matrix_, sizeof(Mat4f));
	return false;
}

void Transform::update() {
	dlg_assert(valid() && ds_ && ubo_.size());
	context().registerUpdateDevice(this);
}

void Transform::bind(const DrawInstance& di) const {
	dlg_assert(valid() && ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), Context::transformBindSet, {ds_}, {});
}

// Scissor
constexpr auto scissorUboSize = sizeof(Vec2f) * 2;
Scissor::Scissor(Context& ctx, const Rect2f& r)
		: DeviceObject(ctx), rect_(r) {

	ubo_ = ctx.device().bufferAllocator().alloc(true, scissorUboSize,
		vk::BufferUsageBits::uniformBuffer);
	ds_ = {ctx.dsLayoutScissor(), ctx.dsPool()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Scissor::update() {
	dlg_assert(valid() && ds_ && ubo_.size());
	context().registerUpdateDevice(this);
}

bool Scissor::updateDevice() {
	dlg_assert(ubo_.size() && ds_);
	auto map = ubo_.memoryMap();
	auto ptr = map.ptr();
	write(ptr, rect_.position);
	write(ptr, rect_.size);
	return false;
}

void Scissor::bind(const DrawInstance& di) const {
	dlg_assert(ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), Context::scissorBindSet, {ds_}, {});
}

// DeviceObject
DeviceObject::DeviceObject(DeviceObject&& rhs) noexcept {
	using std::swap;
	swap(context_, rhs.context_);

	if(context_) {
		context_->deviceObjectMoved(rhs, *this);
	}
}

DeviceObject& DeviceObject::operator=(DeviceObject&& rhs) noexcept {
	if(context_) {
		context_->deviceObjectDestroyed(*this);
	}

	if(rhs.context_) {
		rhs.context_->deviceObjectMoved(rhs, *this);
	}

	context_ = rhs.context_;
	rhs.context_ = {};
	return *this;
}

DeviceObject::~DeviceObject() {
	if(valid()) {
		context().deviceObjectDestroyed(*this);
	}
}

} // namespace rvg
