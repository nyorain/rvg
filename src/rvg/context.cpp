// Copyright (c) 2019 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/context.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>
#include <rvg/polygon.hpp>
#include <rvg/shapes.hpp>
#include <rvg/state.hpp>
#include <rvg/stateChange.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/util.hpp>

#include <katachi/path.hpp>
#include <katachi/curves.hpp>

#include <vpp/vk.hpp>
#include <vpp/queue.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/commandAllocator.hpp>
#include <vpp/submit.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>
#include <nytl/vecOps.hpp>
#include <cstring>
#include <array>

#include <shaders/fill.vert.frag_scissor.h>
#include <shaders/fill.frag.frag_scissor.h>
#include <shaders/fill.vert.plane_scissor.h>
#include <shaders/fill.frag.plane_scissor.h>
#include <shaders/fill.frag.frag_scissor.edge_aa.h>
#include <shaders/fill.frag.plane_scissor.edge_aa.h>

namespace rvg {

// Context
Context::Context(vpp::Device& dev, const ContextSettings& settings) :
		device_(dev), settings_(settings) {

	// sampler
	vk::SamplerCreateInfo samplerInfo {};
	samplerInfo.magFilter = vk::Filter::linear;
	samplerInfo.minFilter = vk::Filter::linear;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.25f;
	samplerInfo.maxAnisotropy = 1.f;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::nearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::clampToEdge;
	samplerInfo.borderColor = vk::BorderColor::floatOpaqueWhite;
	texSampler_ = {dev, samplerInfo};

	// we use linear sampling for fonts as well since normally they are
	// pixel-aligned (and therefore linear == nearest) but when
	// the text is rotatet, linear produces somewhat blurred yet
	// way nicer results than nearest (which renders the text with cuts)
	auto& fontSampler = texSampler_;

	// layouts
	auto transformDSB = std::array {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex),
	};

	auto paintDSB = std::array {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
			vk::ShaderStageBits::vertex | vk::ShaderStageBits::fragment),
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, &texSampler_.vkHandle()),
	};

	auto fontAtlasDSB = std::array {
		vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
			vk::ShaderStageBits::fragment, &fontSampler.vkHandle()),
	};

	auto clipDistance = settings.clipDistanceEnable;
	auto scissorStage = clipDistance ?
		vk::ShaderStageBits::vertex :
		vk::ShaderStageBits::fragment;

	auto scissorDSB = std::array {
		vpp::descriptorBinding(vk::DescriptorType::uniformBuffer, scissorStage),
	};

	dsLayoutTransform_.init(dev, transformDSB);
	dsLayoutPaint_.init(dev, paintDSB);
	dsLayoutFontAtlas_.init(dev, fontAtlasDSB);
	dsLayoutScissor_.init(dev, scissorDSB);
	std::vector<vk::DescriptorSetLayout> layouts = {
		dsLayoutTransform_,
		dsLayoutPaint_,
		dsLayoutFontAtlas_,
		dsLayoutScissor_
	};

	if(settings.antiAliasing) {
		auto aaStrokeDSB = std::array {
			vpp::descriptorBinding(vk::DescriptorType::uniformBuffer,
				vk::ShaderStageBits::fragment),
		};

		dsLayoutStrokeAA_.init(dev, aaStrokeDSB);
		layouts.push_back(dsLayoutStrokeAA_);
	}

	pipeLayout_ = {dev, layouts, {{
		{vk::ShaderStageBits::fragment, 0, 4}
	}}};

	// pipeline
	using ShaderData = nytl::Span<const std::uint32_t>;
	auto vertData = ShaderData(fill_vert_frag_scissor_data);
	auto fragData = ShaderData(fill_frag_frag_scissor_data);

	if(clipDistance) {
		vertData = fill_vert_plane_scissor_data;
		if(settings.antiAliasing) {
			fragData = fill_frag_plane_scissor_edge_aa_data;
		} else {
			fragData = fill_frag_plane_scissor_data;
		}
	} else if(settings.antiAliasing) {
		fragData = fill_frag_frag_scissor_edge_aa_data;
	}

	auto fillVertex = vpp::ShaderModule(dev, vertData);
	auto fillFragment = vpp::ShaderModule(dev, fragData);

	auto samples = settings.samples == vk::SampleCountBits {} ?
		vk::SampleCountBits::e1 : settings.samples;
	vpp::GraphicsPipelineInfo fanPipeInfo(settings.renderPass, pipeLayout_, {{{
		{fillVertex, vk::ShaderStageBits::vertex},
		{fillFragment, vk::ShaderStageBits::fragment}
	}}}, settings.subpass, samples);


	fanPipeInfo.flags(vk::PipelineCreateBits::allowDerivatives);

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

	fanPipeInfo.vertex.pVertexAttributeDescriptions = vertexAttribs.data();
	fanPipeInfo.vertex.vertexAttributeDescriptionCount = vertexAttribs.size();
	fanPipeInfo.vertex.pVertexBindingDescriptions = vertexBindings.data();
	fanPipeInfo.vertex.vertexBindingDescriptionCount = vertexBindings.size();

	fanPipeInfo.assembly.topology = vk::PrimitiveTopology::triangleFan;

	// stripPipe
	auto stripPipeInfo = fanPipeInfo;
	stripPipeInfo.base(0);
	stripPipeInfo.assembly.topology = vk::PrimitiveTopology::triangleStrip;

	auto pipes = vk::createGraphicsPipelines(dev, settings.pipelineCache,
		{{fanPipeInfo.info(), stripPipeInfo.info()}});
	fanPipe_ = {dev, pipes[0]};
	stripPipe_ = {dev, pipes[1]};

	// sync stuff
	auto family = device().queueSubmitter().queue().family();
	uploadSemaphore_ = {device()};
	uploadCmdBuf_ = device().commandAllocator().get(family,
		vk::CommandPoolCreateBits::resetCommandBuffer);

	// dummies
	constexpr std::uint8_t bytes[] = {0xFF, 0xFF, 0xFF, 0xFF};
	auto ptr = reinterpret_cast<const std::byte*>(bytes);
	emptyImage_ = {*this, {1u, 1u}, {ptr, ptr + 4u}, TextureType::rgba32};

	dummyTex_ = {dsAllocator(), dsLayoutFontAtlas_};
	vpp::DescriptorSetUpdate update(dummyTex_);
	auto layout = vk::ImageLayout::shaderReadOnlyOptimal;
	update.imageSampler({{{}, emptyImage_.vkImageView(), layout}});

	identityTransform_ = {*this};
	pointColorPaint_ = {*this, ::rvg::pointColorPaint()};
	defaultScissor_ = {*this, Scissor::reset};
	defaultAtlas_ = std::make_unique<FontAtlas>(*this);

	if(settings.antiAliasing) {
		defaultStrokeAABuf_ = {bufferAllocator(), 12 * sizeof(float),
			vk::BufferUsageBits::uniformBuffer, device().hostMemoryTypes()};
		auto map = defaultStrokeAABuf_.memoryMap();
		auto ptr = map.ptr();
		write(ptr, 1.f);

		defaultStrokeAA_ = {dsAllocator(), dsLayoutStrokeAA_};
		auto& b = defaultStrokeAABuf_;
		vpp::DescriptorSetUpdate update(defaultStrokeAA_);
		update.uniform({{b.buffer(), b.offset(), sizeof(float)}});
	}
}

vpp::DescriptorAllocator& Context::dsAllocator() const {
	return device().descriptorAllocator();
}

vpp::BufferAllocator& Context::bufferAllocator() const {
	return device().bufferAllocator();
}

vpp::DeviceMemoryAllocator& Context::devMemAllocator() const {
	return device().devMemAllocator();
}

void Context::bindDefaults(vk::CommandBuffer cmdb) {
	identityTransform_.bind(cmdb);
	defaultScissor_.bind(cmdb);

	vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
		pipeLayout(), fontBindSet, {{dummyTex_.vkHandle()}}, {});

	if(settings().antiAliasing) {
		vk::cmdBindDescriptorSets(cmdb, vk::PipelineBindPoint::graphics,
			pipeLayout(), aaStrokeBindSet, {{defaultStrokeAA_.vkHandle()}}, {});
	}
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

std::pair<bool, vk::Semaphore> Context::upload(bool submit) {
	auto rerecord = updateDevice();
	auto seph = stageUpload(submit);
	return {rerecord, seph};
}

vk::Semaphore Context::stageUpload(bool submit) {
	vk::Semaphore ret {};
	if(!currentFrame_.cmdBufs.empty()) {
		// we currently record secondary buffers and then unify
		// them here as one since we might need the ordering
		// guarantees. Not really sure though (can currently
		// only think of Texture and that might be solveable
		// in a different way). Investigate/profile.
		vk::beginCommandBuffer(uploadCmdBuf_, {});
		for(auto& buf : currentFrame_.cmdBufs) {
			vk::cmdExecuteCommands(uploadCmdBuf_, {{buf.second.vkHandle()}});
		}
		vk::endCommandBuffer(uploadCmdBuf_);

		auto& qs = device().queueSubmitter();
		vk::SubmitInfo info;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &uploadCmdBuf_.vkHandle();
		info.pSignalSemaphores = &uploadSemaphore_.vkHandle();
		info.signalSemaphoreCount = 1u;
		qs.add(info);
		ret = uploadSemaphore_;

		if(submit) {
			qs.submit();
		}
	}

	oldFrame_ = std::move(currentFrame_);
	return ret;
}

vpp::CommandBuffer Context::uploadCmdBuf() {
	auto family = device().queueSubmitter().queue().family();
	auto flags = vk::CommandPoolCreateBits::resetCommandBuffer |
			vk::CommandPoolCreateBits::transient;
	auto cmd = device().commandAllocator().get(family, flags,
		vk::CommandBufferLevel::secondary);

	vk::CommandBufferInheritanceInfo inherit;
	vk::CommandBufferBeginInfo info;
	info.flags = vk::CommandBufferUsageBits::oneTimeSubmit;
	info.pInheritanceInfo = &inherit;
	vk::beginCommandBuffer(cmd, info);

	return cmd;
}

void Context::addStage(vpp::SubBuffer&& buf) {
	if(buf.size()) {
		currentFrame_.stages.emplace_back(std::move(buf));
	}
}

void Context::addCommandBuffer(DevRes obj, vpp::CommandBuffer&& buf) {
	vk::endCommandBuffer(buf);
	currentFrame_.cmdBufs.emplace_back(obj, std::move(buf));
}

void Context::registerUpdateDevice(DevRes obj) {
	updateDevice_.insert(obj);
}

bool Context::deviceObjectDestroyed(::rvg::DeviceObject& obj) noexcept {
	// remove its command buffers since they might reference
	// resources that just got destroyed.
	auto compareVisitor = [&](auto* ud) {
		return &obj == ud;
	};

	auto& bufs = currentFrame_.cmdBufs;
	bufs.erase(std::remove_if(bufs.begin(), bufs.end(),
		[&](auto& b) { return std::visit(compareVisitor, b.first); }), bufs.end());

	// remove it from updateDevice_ vector
	auto it = updateDevice_.begin();
	auto eraseVisitor = [&](auto* ud) {
		if(&obj == ud) {
			updateDevice_.erase(it);
			return true;
		}

		return false;
	};

	for(; it != updateDevice_.end(); ++it) {
		if(std::visit(eraseVisitor, *it)) {
			return true;
		}
	}

	return false;
}

void Context::deviceObjectMoved(::rvg::DeviceObject& o,
		::rvg::DeviceObject& n) noexcept {

	auto it = updateDevice_.begin();

	// move device object in currentFrame_.cmdBufs
	for(auto& b : currentFrame_.cmdBufs) {
		auto updateVisitor = [&](auto* ud) {
			if(ud == &o) {
				b.first = static_cast<decltype(ud)>(&n);
			}
		};

		std::visit(updateVisitor, b.first);
	}

	// move it in updateDevice_
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

// util
namespace detail {

void outputException(const std::exception& err) {
	dlg_error("~StageChange: object.update() threw: {}", err.what());
}

} // namespace detail
} // namespace rvg
