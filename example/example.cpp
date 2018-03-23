// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include "render.hpp"
#include "window.hpp"
#include "gui.hpp"

#include <vgv/vgv.hpp>
#include <vgv/path.hpp>

#include <ny/backend.hpp>
#include <ny/appContext.hpp>
#include <ny/key.hpp>
#include <ny/mouseButton.hpp>
#include <ny/event.hpp>

#include <vpp/instance.hpp>
#include <vpp/debug.hpp>
#include <vpp/formats.hpp>

#include <nytl/vecOps.hpp>
#include <nytl/matOps.hpp>

#include <dlg/dlg.hpp>

#include <chrono>
#include <array>

using namespace vui;
using namespace nytl::vec::operators;
using namespace nytl::vec::cw::operators;

// settings
constexpr auto appName = "vgv-example";
constexpr auto engineName = "vpp,vgv";
constexpr auto useValidation = true;
constexpr auto startMsaa = vk::SampleCountBits::e1;
constexpr auto layerName = "VK_LAYER_LUNARG_standard_validation";
constexpr auto printFrames = true;
constexpr auto vsync = false;
constexpr auto clearColor = std::array<float, 4>{{0.f, 0.f, 0.f, 1.f}};

// TODO: move to nytl
template<typename T>
void scale(nytl::Mat4<T>& mat, nytl::Vec3<T> fac) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][i] *= fac[i];
	}
}

template<typename T>
void translate(nytl::Mat4<T>& mat, nytl::Vec3<T> move) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][3] += move[i];
	}
}

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

	vgv::Transform transform(ctx);
	scale(transform.matrix, {2.f / window.size().x, 2.f / window.size().y, 1});
	translate(transform.matrix, {-1.f, -1.f, 0.f});
	transform.updateDevice();

	vgv::Shape shape(ctx, {}, {false, 2.f});
	vgv::Paint paint(ctx, {0.1f, .6f, .3f, 1.f});

	auto fontHeight = 12;
	vgv::FontAtlas atlas(ctx);
	vgv::Font osFont(atlas, "../../example/OpenSans-Regular.ttf", fontHeight);
	vgv::Font lsFont(atlas, "../../example/LiberationSans-Regular.ttf", fontHeight);
	atlas.bake(ctx);

	auto string = "yo, whaddup";
	vgv::Text text(ctx, string, lsFont, {0, 0});
	auto textWidth = lsFont.width(string);

	text.updateDevice(ctx);

	// svg path
	auto svgSubpath = vgv::parseSvgSubpath({300, 200},
		"h -150 a150 150 0 1 0 150 -150 z");

	vgv::Shape svgShape(ctx, vgv::bake(svgSubpath), {true, 0.f});

	// gui
	// auto label = vgv::PaintBuffer(ctx, {1.f, 1.f, 1.f, 1.f});
	auto& label = paint.buffer();
	auto normal = vgv::PaintBuffer(ctx, {0.05f, 0.05f, 0.05f, 1.f});
	auto hovered = vgv::PaintBuffer(ctx, {0.07f, 0.07f, 0.07f, 1.f});
	auto pressed = vgv::PaintBuffer(ctx, {0.09f, 0.09f, 0.09f, 1.f});
	Gui::Styles styles {
		{ // button
			{ // normal
				label,
				normal,
			}, { // hovered
				label,
				hovered,
			}, { // pressed
				label,
				pressed
			}
		}, { // textfield
			label,
			normal,
		}
	};

	Gui gui(ctx, lsFont, std::move(styles));

	auto& button = gui.create<Button>(Vec {500.f, 100.f}, "Click me");
	button.onClicked = [](auto&) { dlg_info("Button was clicked"); };
	gui.create<Button>(Vec {500.f, 250.f}, "Button Number Two");
	gui.create<Textfield>(Vec {500.f, 400.f}, 200);
	gui.updateDevice();

	// render recoreding
	renderer.onRender += [&](vk::CommandBuffer buf){
		auto di = ctx.record(buf);

		transform.bind(di);
		paint.bind(di);

		shape.stroke(di);
		svgShape.fill(di);
		text.draw(di);
		gui.draw(di);
	};

	renderer.invalidate();

	// connect window & renderer
	auto run = true;
	window.onClose = [&](const auto&) { run = false; };
	window.onKey = [&](const auto& ev) {
		gui.key({(vui::Key) ev.keycode, ev.pressed});

		if(ev.pressed && !ev.utf8.empty() && !ny::specialKey(ev.keycode)) {
			gui.textInput({ev.utf8.c_str()});
		}

		if(ev.pressed) {
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
			} else if(ev.keycode == ny::Keycode::d) {
				paint.color = {0.1, 0.1, 0.1, 1.f};
				paint.updateDevice();
			} else if(ev.keycode == ny::Keycode::w) {
				paint.color = {1, 1, 1, 1.f};
				paint.updateDevice();
			} if(ev.keycode == ny::Keycode::k1) {
				text.font = &lsFont;
				text.update();
				text.updateDevice(ctx);
			} else if(ev.keycode == ny::Keycode::k2) {
				text.font = &osFont;
				text.update();
				text.updateDevice(ctx);
			}
		}
	};
	window.onResize = [&](const auto& ev) {
		renderer.resize(ev.size);

		text.pos.x = (ev.size[0] - textWidth) / 2;
		text.pos.y = ev.size[1] - fontHeight - 20;
		text.update();
		if(text.updateDevice(ctx)) {
			dlg_warn("unexpected text rerecord");
		}

		transform.matrix = nytl::identity<4, float>();
		auto s = nytl::Vec {2.f / window.size().x, 2.f / window.size().y, 1};
		scale(transform.matrix, s);
		translate(transform.matrix, {-1, -1, 0});
		transform.updateDevice();
	};

	vgv::Subpath subpath;
	bool first = true;

	window.onMouseButton = [&](const auto& ev) {
		if(gui.mouseButton({ev.pressed,
				static_cast<vui::MouseButton>(ev.button),
				static_cast<Vec2f>(ev.position)}) || !ev.pressed) {
			return;
		}

		auto p = static_cast<nytl::Vec2f>(ev.position);
		if(first) {
			first = false;
			subpath.start = p;
		} else {
			subpath.commands.push_back({p, vgv::SQBezierParams {}});
			shape.points = vgv::bake(subpath);
			shape.update();
			if(shape.updateDevice(ctx)) {
				dlg_info("rerecord");
				renderer.invalidate();
			}
		}
	};

	window.onMouseMove = [&](const auto& ev) {
		gui.mouseMove({static_cast<nytl::Vec2f>(ev.position)});
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

		gui.update(deltaCount);

		if(gui.updateDevice()) {
			renderer.invalidate();
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
