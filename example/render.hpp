// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <vpp/device.hpp> // vpp::Device
#include <vpp/queue.hpp> // vpp::Queue
#include <vpp/buffer.hpp> // vpp::Buffer
#include <vpp/renderer.hpp> // vpp::DefaultRenderer
#include <vpp/commandBuffer.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/framebuffer.hpp>
#include <vpp/queue.hpp>
#include <vpp/vk.hpp>
#include <nytl/vec.hpp>
#include <nytl/callback.hpp>

struct RendererCreateInfo {
	const vpp::Device& dev;
	vk::SurfaceKHR surface;
	nytl::Vec2ui size;
	const vpp::Queue& present;

	vk::SampleCountBits samples = vk::SampleCountBits::e1;
	bool vsync = false;
	std::array<float, 4> clearColor = {0.f, 0.f, 0.f, 1.f};
};

class Renderer : public vpp::DefaultRenderer {
public:
	nytl::Callback<void(vk::CommandBuffer)> onRender;

public:
	Renderer(const RendererCreateInfo& info);
	~Renderer() = default;

	void resize(nytl::Vec2ui size);
	void samples(vk::SampleCountBits);

	vk::RenderPass renderPass() const { return renderPass_; }
	vk::SampleCountBits samples() const { return sampleCount_; }

protected:
	void createMultisampleTarget(const vk::Extent2D& size);
	void record(const RenderBuffer&) override;
	void initBuffers(const vk::Extent2D&, nytl::Span<RenderBuffer>) override;

protected:
	vpp::ViewableImage multisampleTarget_;
	vpp::RenderPass renderPass_;
	vk::SampleCountBits sampleCount_;
	vk::SwapchainCreateInfoKHR scInfo_;
	std::array<float, 4> clearColor_;
};
