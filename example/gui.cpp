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
Gui::Gui(Context& ctx, const Font& font, Styles&& s, GuiListener& listener)
	: styles(std::move(s)), context_(ctx), font_(font), listener_(listener) {
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
	if(!ev.pressed && buttonGrab_.first && ev.button == buttonGrab_.second) {
		buttonGrab_.first->mouseButton(ev);
		buttonGrab_ = {};
	} else if(mouseOver_) {
		if(ev.pressed) {
			buttonGrab_ = {mouseOver_, ev.button};
		}

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
void Gui::draw(const DrawInstance& di) {
	for(auto& widget : widgets_) {
		dlg_assert(widget);
		widget->draw(di);
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
Button::Button(Gui& gui, Vec2f pos, std::string label) : Widget(gui) {

	auto& ctx = gui.context();
	auto& font = gui.font();
	auto padding = Vec {40, 15};
	draw_.label.text = {label, font, pos + padding};
	draw_.label.paint = {ctx, gui.styles.button.normal.label.get()};
	auto size = 2 * padding + Vec {font.width(label), font.height()};
	draw_.bg.shape = {pos, size, DrawMode {true, 2.f}};
	draw_.bg.paint = {ctx, gui.styles.button.normal.bg.get()};

	bounds = {pos, size};
	draw_.label.text.updateDevice(ctx);
	draw_.bg.shape.updateDevice(ctx);
}

void Button::mouseButton(const MouseButtonEvent& event) {
	if(event.button != 2) {
		return;
	}

	if(event.pressed) {
		pressed_ = true;
	} else if(pressed_) {
		pressed_ = false;
		if(hovered_) {
			onClicked(*this);
		}
	}

	registerUpdateDevice();
}
void Button::mouseOver(bool gained) {
	if(gained != hovered_) {
		registerUpdateDevice();
		gui.rerecord();
	}
	hovered_ = gained;
}

void Button::draw(const DrawInstance& di) const {
	draw_.bg.paint.bind(di);
	draw_.bg.shape.fill(di);

	draw_.label.paint.bind(di);
	draw_.label.text.draw(di);

	draw_.bg.shape.stroke(di);
}

bool Button::updateDevice() const {
	auto& bs = gui.styles.button;
	auto& draw = pressed_ ? bs.pressed : hovered_ ? bs.hovered : bs.normal;

	draw_.bg.paint.updateDevice(draw.bg.get());
	draw_.label.paint.updateDevice(draw.label.get());

	return false;
}

// Textfield
Textfield::Textfield(Gui& gui, Vec2f pos, float width) : Widget(gui) {
	auto& font = gui.font();
	auto height = font.height() + 10.f;
	draw_.bg.shape = {pos, {width, height}, {true, 2.f}};
	draw_.cursor.shape = {};
}

void Textfield::mouseButton(const MouseButtonEvent&) {
}
void Textfield::focus(bool gained) {
}
void Textfield::textInput(const TextInputEvent&) {
}
void Textfield::key(const KeyEvent&) {
}

void Textfield::draw(const DrawInstance&) const {
}

bool Textfield::updateDevice() const {
}

} // namespace vui
