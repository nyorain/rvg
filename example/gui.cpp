#include "gui.hpp"
#include <dlg/dlg.hpp>
#include <nytl/rectOps.hpp>

namespace vui {

// GuiListener
GuiListener& GuiListener::nop() {
	static GuiListener listener;
	return listener;
}

// Gui
Gui::Gui(Context& context, const Font& font, GuiListener& listener)
	: context_(context), font_(font), listener_(listener) {
}

void Gui::mouseMove(const MouseMoveEvent& ev) {
	for(auto& w : widgets_) {
		if(contains(w->bounds, ev.position)) {
			if(w.get() != mouseOver_) {
				if(mouseOver_) {
					mouseOver_->mouseOver(false);
				}

				listener().mouseOver(mouseOver_, w.get());
				mouseOver_ = w.get();
				w->mouseOver(true);
			}

			w->mouseOver(true);
			return;
		}
	}

	if(mouseOver_) {
		mouseOver_->mouseOver(false);
		listener().mouseOver(mouseOver_, nullptr);
		mouseOver_ = nullptr;
	}
}
void Gui::mouseButton(const MouseButtonEvent& ev) {
	if(mouseOver_) {
		mouseOver_->mouseButton(ev);
	}
}
void Gui::key(const KeyEvent&) {
}
void Gui::textInput(const TextInputEvent&) {
}
void Gui::focus(bool gained) {
	((void) gained);
}
void Gui::mouseOver(bool gained) {
	if(!gained && mouseOver_) {
		mouseOver_->mouseOver(false);
		listener().mouseOver(mouseOver_, nullptr);
		mouseOver_ = nullptr;
	}
}

// NOTE: can be optimized (moved vectors could be cached for less
// allocations).
void Gui::update(double delta) {
	auto moved = std::move(update_);
	for(auto& widget : moved) {
		dlg_assert(widget);
		widget->update(delta);
	}
}
bool Gui::updateDevice() {
	auto moved = std::move(updateDevice_);
	bool rerecord = false;
	for(auto& widget : moved) {
		dlg_assert(widget);
		rerecord |= widget->updateDevice();
	}

	return rerecord || rerecord_;
}
void Gui::draw(vk::CommandBuffer cmdb) {
	for(auto& widget : widgets_) {
		dlg_assert(widget);
		widget->draw(cmdb);
	}
}

Widget& Gui::add(std::unique_ptr<Widget> widget) {
	dlg_assert(widget);
	widgets_.push_back(std::move(widget));
	return *widgets_.back();
}

void Gui::addUpdate(Widget& widget) {
	update_.push_back(&widget);
}
void Gui::addUpdateDevice(Widget& widget) {
	updateDevice_.push_back(&widget);
}

// Widget
void Widget::registerUpdate() {
	gui.addUpdate(*this);
}
void Widget::registerUpdateDevice() {
	gui.addUpdateDevice(*this);
}

// Button
Button::Button(Gui& gui, Vec2f pos, std::string xlabel) :
		Widget(gui), label(std::move(xlabel)) {

	auto& ctx = gui.context();
	auto& font = gui.font();
	auto padding = Vec {40, 15};
	draw_.label = {ctx, label, font, pos + padding};
	auto size = 2 * padding + Vec {font.width(label), font.height()};

	draw_.bg = {gui.context()};
	draw_.bg.points = {
		pos,
		pos + Vec {size.x, 0},
		pos + size,
		pos + Vec {0, size.y}
	};

	bounds = {pos, size};
	registerUpdateDevice();
}

void Button::mouseButton(const MouseButtonEvent& event) {
	if(event.button != 2) {
		return;
	}

	if(event.pressed) {
		pressed_ = true;
	} else if(pressed_) {
		pressed_ = false;
		onClicked(*this);
	}

	registerUpdateDevice();
	gui.rerecord();
}
void Button::mouseOver(bool gained) {
	if(gained != hovered_) {
		registerUpdateDevice();
		gui.rerecord();
	}
	hovered_ = gained;
}

void Button::draw(vk::CommandBuffer cmdb) {
	auto& ctx = gui.context();
	auto& bs = gui.styles.button;
	auto& draw = pressed_ ? bs.pressed : hovered_ ? bs.hovered : bs.normal;
	draw.bg.bind(ctx, cmdb);
	draw_.bg.draw(ctx, cmdb);

	draw.label.bind(ctx, cmdb);
	draw_.label.draw(ctx, cmdb);
}
bool Button::updateDevice() {
	auto& ctx = gui.context();
	return draw_.bg.updateDevice(ctx) | draw_.label.updateDevice(ctx);
}

} // namespace vui
