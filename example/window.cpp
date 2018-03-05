// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#define DLG_DEFAULT_TAGS "window",

#include "window.hpp"

#include <ny/key.hpp> // ny::Keycode
#include <ny/mouseButton.hpp> // ny::MouseButton
#include <ny/image.hpp> // ny::Image
#include <ny/event.hpp> // ny::*Event
#include <ny/keyboardContext.hpp> // ny::KeyboardContext
#include <ny/appContext.hpp> // ny::AppContext
#include <ny/windowContext.hpp> // ny::WindowContext
#include <ny/windowSettings.hpp> // ny::WindowEdge
#include <nytl/vecOps.hpp>
#include <dlg/dlg.hpp>

MainWindow::MainWindow(ny::AppContext& ac, vk::Instance ini) : ac_(ac)
{
	auto ws = ny::WindowSettings {};

	ws.surface = ny::SurfaceType::vulkan;
	ws.listener = this;
	ws.transparent = false;
	ws.vulkan.instance = reinterpret_cast<const VkInstance&>(ini);
	ws.vulkan.storeSurface = &reinterpret_cast<std::uintptr_t&>(surface_);

	wc_ = ac.createWindowContext(ws);
}

MainWindow::~MainWindow()
{
}

void MainWindow::mouseButton(const ny::MouseButtonEvent& ev)
{
	if(ev.pressed) {
		if(ev.button == ny::MouseButton::left) {
			auto mods = appContext().keyboardContext()->modifiers();
			bool alt =  mods & ny::KeyboardModifier::alt;

			if(alt) {
				unsigned int x = ev.position[0];
				unsigned int y = ev.position[1];

				bool outside = ev.position[0] < 0 || ev.position[1] < 0;
				outside |= x > size_[0] || y > size_[1];

				if(outside) {
					dlg_warn("MouseButton events outside of bounds");
					return;
				}

				ny::WindowEdges edges = ny::WindowEdge::none;
				if(x < 100) {
					edges |= ny::WindowEdge::left;
				} else if(x > size_[0] - 100) {
					edges |= ny::WindowEdge::right;
				}

				if(y < 100) {
					edges |= ny::WindowEdge::top;
				} else if(y > size_[1] - 100) {
					edges |= ny::WindowEdge::bottom;
				}

				if(edges != ny::WindowEdge::none) {
					dlg_debug("Starting to resize window");
					windowContext().beginResize(ev.eventData, edges);
				} else {
					dlg_debug("Starting to move window");
					windowContext().beginMove(ev.eventData);
				}
			}
		}
	}

	onMouseButton(ev);
}
void MainWindow::mouseWheel(const ny::MouseWheelEvent& event)
{
	if(onMouseWheel) {
		onMouseWheel(event);
	}
}
void MainWindow::mouseMove(const ny::MouseMoveEvent& event)
{
	if(onMouseMove) {
		onMouseMove(event);
	}
}
void MainWindow::mouseCross(const ny::MouseCrossEvent& event)
{
	mouseOver_ = event.entered;
}
void MainWindow::key(const ny::KeyEvent& ev)
{
	if(onKey) {
		onKey(ev);
	}

	auto keycode = ev.keycode;
	auto mods = appContext().keyboardContext()->modifiers();
	if(ev.pressed && (mods & ny::KeyboardModifier::shift)) {
		if(keycode == ny::Keycode::f) {
			dlg_debug("f pressed. Fullscreen");
			if(state_ != ny::ToplevelState::fullscreen) {
				windowContext().fullscreen();
				state_ = ny::ToplevelState::fullscreen;
			} else {
				windowContext().normalState();
				state_ = ny::ToplevelState::normal;
			}
		} else if(keycode == ny::Keycode::n) {
			dlg_debug("n pressed. Resetting to normal state");
			windowContext().normalState();
		} else if(keycode == ny::Keycode::m) {
			dlg_debug("m pressed. Toggle maximize");
			if(state_ != ny::ToplevelState::maximized) {
				windowContext().maximize();
				state_ = ny::ToplevelState::maximized;
			} else {
				windowContext().normalState();
				state_ = ny::ToplevelState::normal;
			}
		} else if(keycode == ny::Keycode::i) {
			dlg_debug("i pressed. Toggle iconic (minimize)");
			state_ = ny::ToplevelState::minimized;
			windowContext().minimize();
		} else if(keycode == ny::Keycode::d) {
			dlg_debug("d pressed. Trying to toggle decorations");
			auto cd = windowContext().customDecorated();
			windowContext().customDecorated(!cd);
		}
	}
}
void MainWindow::state(const ny::StateEvent& ev)
{
	dlg_debug("Window state changed to {}", (unsigned int) ev.state);
	if(ev.state != state_) {
		state_ = ev.state;
	}
}
void MainWindow::close(const ny::CloseEvent& ev)
{
	dlg_debug("Window closed");
	if(onClose) {
		onClose(ev);
	}
}
void MainWindow::resize(const ny::SizeEvent& ev)
{
	dlg_debug("Window resized to {}", ev.size);
	size_ = ev.size;
	if(onResize) {
		onResize(ev);
	}
}

void MainWindow::focus(const ny::FocusEvent& ev)
{
	dlg_debug("focus: {}", ev.gained);
	focus_ = ev.gained;
	if(onFocus) {
		onFocus(ev);
	}
}

void MainWindow::surfaceDestroyed(const ny::SurfaceDestroyedEvent&)
{
	surface_ = {};
	if(onSurfaceDestroyed) {
		onSurfaceDestroyed();
	}
}

void MainWindow::surfaceCreated(const ny::SurfaceCreatedEvent& ev)
{
	surface_ = reinterpret_cast<const vk::SurfaceKHR&>(ev.surface.vulkan);
	if(onSurfaceCreated) {
		onSurfaceCreated(surface_);
	}
}
