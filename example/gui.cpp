#include "gui.hpp"
#include <dlg/dlg.hpp>
#include <nytl/rectOps.hpp>
#include <nytl/utf.hpp>
#include <cmath>

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
bool Gui::mouseButton(const MouseButtonEvent& ev) {
	if(!ev.pressed && buttonGrab_.first && ev.button == buttonGrab_.second) {
		buttonGrab_.first->mouseButton(ev);
		buttonGrab_ = {};
	} else {
		if(mouseOver_ != focus_) {
			if(focus_) {
				focus_->focus(false);
				listener().focus(focus_, mouseOver_);
			}

			focus_ = mouseOver_;
			if(focus_) {
				focus_->focus(true);
			}
		}

		if(mouseOver_) {
			if(ev.pressed) {
				buttonGrab_ = {mouseOver_, ev.button};
			}

			mouseOver_->mouseButton(ev);
		}
	}

	return mouseOver_;
}
void Gui::key(const KeyEvent& ev) {
	if(focus_) {
		focus_->key(ev);
	}
}
void Gui::textInput(const TextInputEvent& ev) {
	if(focus_) {
		focus_->textInput(ev);
	}
}
void Gui::focus(bool gained) {
	if(!gained && focus_) {
		focus_->focus(false);
		listener().focus(focus_, nullptr);
		focus_ = nullptr;
	}
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
	update_.insert(&widget);
}
void Gui::addUpdateDevice(Widget& widget) {
	updateDevice_.insert(&widget);
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
	draw_.label.text = {ctx, label, font, pos + padding};
	draw_.label.paint = {ctx, gui.styles.button.normal.label.get()};
	auto size = 2 * padding + Vec {font.width(label), font.height()};
	draw_.bg.shape = {ctx, pos, size, DrawMode {true, 2.f}};
	draw_.bg.paint = {ctx, gui.styles.button.normal.bg.get()};

	bounds = {pos, size};
	draw_.label.text.updateDevice(ctx);
	draw_.bg.shape.updateDevice(ctx);
}

void Button::mouseButton(const MouseButtonEvent& event) {
	if(event.button != MouseButton::left) {
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

bool Button::updateDevice() {
	auto& bs = gui.styles.button;
	auto& draw = pressed_ ? bs.pressed : hovered_ ? bs.hovered : bs.normal;

	draw_.bg.paint.updateDevice(draw.bg.get());
	draw_.label.paint.updateDevice(draw.label.get());

	return false;
}

// Textfield
Textfield::Textfield(Gui& gui, Vec2f pos, float width) : Widget(gui) {
	auto& ctx = gui.context();
	auto padding = Vec {10.f, 10.f};

	auto& font = gui.font();
	auto height = font.height() + 2 * padding.y;
	draw_.bg.shape = {ctx, pos, {width, height}, {true, 2.f}};
	draw_.bg.paint = {ctx, gui.styles.textfield.bg};

	auto cursorSize = Vec {1.f, font.height() + padding.y};
	auto cursorPos = pos + Vec {padding.x, 0.5f * padding.y};
	draw_.cursor.shape = {ctx, cursorPos, cursorSize, {true, 0.f}, true};

	draw_.label.text = {ctx, "", font, pos + padding};
	draw_.label.paint = {ctx, gui.styles.textfield.label};

	this->bounds = {pos, {width, height}};
}

void Textfield::mouseButton(const MouseButtonEvent& ev) {
	auto& text = draw_.label.text;
	auto ca = text.charAt(ev.position.x - text.pos.x);
	cursor_ = ca.last;
	draw_.cursor.shape.pos.x = text.pos.x + ca.nearestBoundary;
	draw_.cursor.shape.update();
	registerUpdateDevice();
	dlg_assert(cursor_ <= text.text.length());
}

void Textfield::focus(bool gained) {
	focus_ = gained;
	cursor(focus_, true);
	registerUpdate();
}
void Textfield::textInput(const TextInputEvent& ev) {
	auto utf32 = toUtf32(ev.utf8);
	auto& str = draw_.label.text.text;
	dlg_assert(cursor_ <= str.length());
	str.insert(cursor_, utf32);
	draw_.label.text.update();
	cursor_ += utf32.length();
	updateCursorPosition();
	dlg_assert(cursor_ <= str.length());
	cursor(true, true);
}

void Textfield::key(const KeyEvent& ev) {
	auto& str = draw_.label.text.text;
	dlg_assert(cursor_ <= str.length());

	if(ev.pressed) {
		if(ev.key == Key::backspace && cursor_ > 0) {
			cursor_ -= 1;
			str.erase(cursor_, 1);
			draw_.label.text.update();
			updateCursorPosition();
		} else if(ev.key == Key::left && cursor_ > 0) {
			cursor_ -= 1;
			updateCursorPosition();
			cursor(true, true);
		} else if(ev.key == Key::right && cursor_ < str.length()) {
			cursor_ += 1;
			updateCursorPosition();
			cursor(true, true);
		} else if(ev.key == Key::del && cursor_ < str.length()) {
			str.erase(cursor_, 1);
			draw_.label.text.update();
			registerUpdateDevice();
		}
	}

	dlg_assert(cursor_ <= str.length());
}

void Textfield::draw(const DrawInstance& di) const {
	draw_.bg.paint.bind(di);
	draw_.bg.shape.fill(di);

	draw_.label.paint.bind(di);
	draw_.label.text.draw(di);

	draw_.cursor.shape.fill(di);

	draw_.bg.shape.stroke(di);
}

bool Textfield::updateDevice() {
	auto& ctx = gui.context();
	return draw_.label.text.updateDevice(ctx) |
		draw_.cursor.shape.updateDevice(ctx);
}

void Textfield::update(double delta) {
	constexpr auto blinkTime = 0.5;
	blinkAccum_ = blinkAccum_ + delta;
	if(blinkAccum_ > blinkTime) {
		blinkAccum_ = std::fmod(blinkAccum_, blinkTime);
		draw_.cursor.shape.hide ^= true;
		registerUpdateDevice();
	}

	if(focus_) {
		registerUpdate();
	}
}

void Textfield::updateCursorPosition() {
	auto len = draw_.label.text.text.size();
	dlg_assert(cursor_ <= len);
	auto x = bounds.position.x + 10.f; // padding
	if(cursor_ > 0 && cursor_ == len) {
		auto b = draw_.label.text.ithBounds(cursor_ - 1);
		x += b.position.x + b.size.x;
	} else if(cursor_ > 0) {
		x += draw_.label.text.ithBounds(cursor_).position.x;
	}

	draw_.cursor.shape.pos.x = x;
	draw_.cursor.shape.update();

	registerUpdateDevice();
}

void Textfield::cursor(bool show, bool resetBlink) {
	draw_.cursor.shape.hide = !show;
	registerUpdateDevice();
	if(resetBlink) {
		blinkAccum_ = 0.f;
	}
}

} // namespace vui
