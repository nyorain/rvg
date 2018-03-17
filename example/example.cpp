// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include "render.hpp"
#include "window.hpp"

#include <vgv/vgv.hpp>

#include <ny/backend.hpp>
#include <ny/appContext.hpp>
#include <ny/key.hpp>
#include <ny/mouseButton.hpp>
#include <ny/event.hpp>
#include <vpp/instance.hpp>
#include <vpp/debug.hpp>
#include <nytl/vecOps.hpp>
#include <dlg/dlg.hpp>

#include <chrono>
#include <array>

// settings
constexpr auto appName = "vgv-example";
constexpr auto engineName = "vpp,vgv";
constexpr auto useValidation = true;
constexpr auto startMsaa = vk::SampleCountBits::e1;
constexpr auto layerName = "VK_LAYER_LUNARG_standard_validation";
constexpr auto printFrames = true;
constexpr auto vsync = true;
constexpr auto clearColor = std::array<float, 4>{{1.0f, 1.0f, 1.0f, 1.f}};

int main() {
	// - initialization -
	auto& backend = ny::Backend::choose();
	if(!backend.vulkan()) {
		throw std::runtime_error("ny backend has no vulkan support!");
	}

	auto appContext = backend.createAppContext();

	// vulkan init: instance
	auto iniExtensions = appContext->vulkanExtensions();
	iniExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	vk::ApplicationInfo appInfo (appName, 1, engineName, 1, VK_API_VERSION_1_0);
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = &appInfo;

	instanceInfo.enabledExtensionCount = iniExtensions.size();
	instanceInfo.ppEnabledExtensionNames = iniExtensions.data();

	if(useValidation) {
		instanceInfo.enabledLayerCount = 1;
		instanceInfo.ppEnabledLayerNames = &layerName;
	}

	vpp::Instance instance {};
	try {
		instance = {instanceInfo};
		if(!instance.vkInstance()) {
			throw std::runtime_error("vkCreateInstance returned a nullptr");
		}
	} catch(const std::exception& error) {
		dlg_error("Vulkan instance creation failed: {}", error.what());
		dlg_error("\tYour system may not support vulkan");
		dlg_error("\tThis application requires vulkan to work");
		throw;
	}

	// debug callback
	std::unique_ptr<vpp::DebugCallback> debugCallback;
	if(useValidation) {
		debugCallback = std::make_unique<vpp::DebugCallback>(instance);
	}

	// init ny window
	MainWindow window(*appContext, instance);
	auto vkSurf = window.vkSurface();

	const vpp::Queue* presentQueue {};
	auto device = vpp::Device(instance, vkSurf, presentQueue);

	auto renderInfo = RendererCreateInfo {
		device, vkSurf, window.size(), *presentQueue,
		startMsaa, vsync, clearColor
	};
	auto renderer = Renderer(renderInfo);

	// vgv
	vgv::Context ctx(device, renderer.renderPass(), 0);
	vgv::Polygon polygon(ctx, true);
	polygon.updateDevice(ctx, vgv::DrawMode::fill);

	vgv::Paint paint(ctx, {0.1f, .6f, .3f, 1.f});

	renderer.onRender += [&](vk::CommandBuffer buf){
		vk::cmdBindDescriptorSets(buf, vk::PipelineBindPoint::graphics,
			ctx.pipeLayout(), 1, {ctx.dummyTex()}, {});
		paint.bind(ctx, buf);
		polygon.fill(ctx, buf);
	};

	renderer.invalidate();

	// connect window & renderer
	auto run = true;
	window.onClose = [&](const auto&) { run = false; };
	window.onKey = [&](const auto& ev) {
		if(!ev.pressed) {
			return;
		}

		if(ev.keycode == ny::Keycode::escape) {
			dlg_info("Escape pressed, exiting");
			run = false;
		} else if(ev.keycode == ny::Keycode::b) {
			paint.color = {0.2, 0.2, 0.8, 1.f};
			paint.updateDevice();
		} else if(ev.keycode == ny::Keycode::g) {
			paint.color = {0.1, 0.6, 0.3, 1.f};
			paint.updateDevice();
		} else if(ev.keycode == ny::Keycode::r) {
			paint.color = {0.8, 0.2, 0.3, 1.f};
			paint.updateDevice();
		}
	};
	window.onResize = [&](const auto& ev) {
		renderer.resize(ev.size);
	};
	window.onMouseButton = [&](const auto& ev) {
		if(!ev.pressed) {
			return;
		}

		float x = ev.position[0] / float(window.size()[0]);
		float y = ev.position[1] / float(window.size()[1]);
		polygon.points().push_back({x, y}); // pos
		polygon.points().push_back({0, 0}); // (unused) uv
		if(polygon.updateDevice(ctx, vgv::DrawMode::fill)) {
			dlg_info("rerecord");
			renderer.invalidate();
		}
	};

	// - main loop -
	using Clock = std::chrono::high_resolution_clock;
	using Secf = std::chrono::duration<float, std::ratio<1, 1>>;

	auto lastFrame = Clock::now();
	auto fpsCounter = 0u;
	auto secCounter = 0.f;

	while(run) {
		auto now = Clock::now();
		auto diff = now - lastFrame;
		auto deltaCount = std::chrono::duration_cast<Secf>(diff).count();
		lastFrame = now;

		if(!appContext->pollEvents()) {
			dlg_info("pollEvents returned false");
			return 0;
		}

		renderer.renderBlock();

		if(printFrames) {
			++fpsCounter;
			secCounter += deltaCount;
			if(secCounter >= 1.f) {
				dlg_info("{} fps", fpsCounter);
				secCounter = 0.f;
				fpsCounter = 0;
			}
		}
	}
}
