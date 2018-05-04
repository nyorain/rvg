// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include "render.hpp"

#include <nytl/mat.hpp>
#include <vpp/vk.hpp>
#include <vpp/util/file.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/swapchain.hpp>
#include <vpp/formats.hpp>

#include <dlg/dlg.hpp> // dlg

vpp::RenderPass createRenderPass(const vpp::Device&, vk::Format,
	vk::SampleCountBits);

Renderer::Renderer(const RendererCreateInfo& info) :
	DefaultRenderer(info.present), sampleCount_(info.samples),
		clearColor_(info.clearColor) {

	vpp::SwapchainPreferences prefs {};
	if(info.vsync) {
		prefs.presentMode = vk::PresentModeKHR::fifo; // vsync
	}

	scInfo_ = vpp::swapchainCreateInfo(info.dev, info.surface,
		{info.size[0], info.size[1]}, prefs);
	renderPass_ = createRenderPass(info.dev, scInfo_.imageFormat, samples());
	vpp::DefaultRenderer::init(renderPass_, scInfo_);
}

void Renderer::createMultisampleTarget(const vk::Extent2D& size)
{
	auto width = size.width;
	auto height = size.height;

	// img
	vk::ImageCreateInfo img;
	img.imageType = vk::ImageType::e2d;
	img.format = scInfo_.imageFormat;
	img.extent.width = width;
	img.extent.height = height;
	img.extent.depth = 1;
	img.mipLevels = 1;
	img.arrayLayers = 1;
	img.sharingMode = vk::SharingMode::exclusive;
	img.tiling = vk::ImageTiling::optimal;
	img.samples = sampleCount_;
	img.usage = vk::ImageUsageBits::transientAttachment | vk::ImageUsageBits::colorAttachment;
	img.initialLayout = vk::ImageLayout::undefined;

	// view
	vk::ImageViewCreateInfo view;
	view.viewType = vk::ImageViewType::e2d;
	view.format = img.format;
	view.components.r = vk::ComponentSwizzle::r;
	view.components.g = vk::ComponentSwizzle::g;
	view.components.b = vk::ComponentSwizzle::b;
	view.components.a = vk::ComponentSwizzle::a;
	view.subresourceRange.aspectMask = vk::ImageAspectBits::color;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;

	// create the viewable image
	// will set the created image in the view info for us
	multisampleTarget_ = {device(), img, view};
}

void Renderer::record(const RenderBuffer& buf)
{
	const auto width = scInfo_.imageExtent.width;
	const auto height = scInfo_.imageExtent.height;
	const auto clearValue = vk::ClearValue {{
		clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]
	}};

	auto cmdBuf = buf.commandBuffer;
	vk::beginCommandBuffer(cmdBuf, {});

	vk::cmdBeginRenderPass(cmdBuf, {
		renderPass(),
		buf.framebuffer,
		{0u, 0u, width, height},
		1,
		&clearValue
	}, {});

	vk::Viewport vp {0.f, 0.f, (float) width, (float) height, 0.f, 1.f};
	vk::cmdSetViewport(cmdBuf, 0, 1, vp);
	vk::cmdSetScissor(cmdBuf, 0, 1, {0, 0, width, height});

	onRender(cmdBuf);

	vk::cmdEndRenderPass(cmdBuf);
	vk::endCommandBuffer(cmdBuf);
}

void Renderer::resize(nytl::Vec2ui size)
{
	vpp::DefaultRenderer::recreate({size[0], size[1]}, scInfo_);
}

void Renderer::samples(vk::SampleCountBits samples)
{
	sampleCount_ = samples;
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
	}

	renderPass_ = createRenderPass(device(), scInfo_.imageFormat, samples);
	vpp::DefaultRenderer::renderPass_ = renderPass_;

	initBuffers(scInfo_.imageExtent, renderBuffers_);
	invalidate();
}

void Renderer::initBuffers(const vk::Extent2D& size,
	nytl::Span<RenderBuffer> bufs)
{
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
		vpp::DefaultRenderer::initBuffers(size, bufs,
			{multisampleTarget_.vkImageView()});
	} else {
		vpp::DefaultRenderer::initBuffers(size, bufs, {});
	}
}

// util
vpp::RenderPass createRenderPass(const vpp::Device& dev,
	vk::Format format, vk::SampleCountBits sampleCount)
{
	vk::AttachmentDescription attachments[2] {};
	auto msaa = sampleCount != vk::SampleCountBits::e1;

	auto swapchainID = 0u;
	if(msaa) {
		// multisample color attachment
		attachments[0].format = format;
		attachments[0].samples = sampleCount;
		attachments[0].loadOp = vk::AttachmentLoadOp::clear;
		attachments[0].storeOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].initialLayout = vk::ImageLayout::undefined;
		attachments[0].finalLayout = vk::ImageLayout::presentSrcKHR;

		swapchainID = 1u;
	}

	// swapchain color attachments we want to resolve to
	attachments[swapchainID].format = format;
	attachments[swapchainID].samples = vk::SampleCountBits::e1;
	if(msaa) attachments[swapchainID].loadOp = vk::AttachmentLoadOp::dontCare;
	else attachments[swapchainID].loadOp = vk::AttachmentLoadOp::clear;
	attachments[swapchainID].storeOp = vk::AttachmentStoreOp::store;
	attachments[swapchainID].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
	attachments[swapchainID].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
	attachments[swapchainID].initialLayout = vk::ImageLayout::undefined;
	attachments[swapchainID].finalLayout = vk::ImageLayout::presentSrcKHR;

	// refs
	vk::AttachmentReference colorReference;
	colorReference.attachment = 0;
	colorReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	vk::AttachmentReference resolveReference;
	resolveReference.attachment = 1;
	resolveReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	// deps
	std::vector<vk::SubpassDependency> dependencies;

	if(msaa) {
		dependencies.resize(2);

		dependencies[0].srcSubpass = vk::subpassExternal;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = vk::PipelineStageBits::bottomOfPipe;
		dependencies[0].dstStageMask = vk::PipelineStageBits::colorAttachmentOutput;
		dependencies[0].srcAccessMask = vk::AccessBits::memoryRead;
		dependencies[0].dstAccessMask = vk::AccessBits::colorAttachmentRead |
			vk::AccessBits::colorAttachmentWrite;
		dependencies[0].dependencyFlags = vk::DependencyBits::byRegion;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = vk::subpassExternal;
		dependencies[1].srcStageMask = vk::PipelineStageBits::colorAttachmentOutput;;
		dependencies[1].dstStageMask = vk::PipelineStageBits::bottomOfPipe;
		dependencies[1].srcAccessMask = vk::AccessBits::colorAttachmentRead |
			vk::AccessBits::colorAttachmentWrite;
		dependencies[1].dstAccessMask = vk::AccessBits::memoryRead;
		dependencies[1].dependencyFlags = vk::DependencyBits::byRegion;
	}

	// only subpass
	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::graphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	if(sampleCount != vk::SampleCountBits::e1)
		subpass.pResolveAttachments = &resolveReference;

	vk::SubpassDependency dependency;
	dependency.srcSubpass = vk::subpassExternal;
	dependency.srcStageMask =
		vk::PipelineStageBits::host |
		vk::PipelineStageBits::transfer;
	dependency.srcAccessMask = vk::AccessBits::hostWrite |
		vk::AccessBits::transferWrite;
	dependency.dstSubpass = 0u;
	dependency.dstStageMask = vk::PipelineStageBits::allGraphics;
	dependency.dstAccessMask = vk::AccessBits::uniformRead |
		vk::AccessBits::vertexAttributeRead |
		vk::AccessBits::indirectCommandRead |
		vk::AccessBits::shaderRead;
	dependencies.push_back(dependency);

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = 1 + msaa;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	return {dev, renderPassInfo};
}
