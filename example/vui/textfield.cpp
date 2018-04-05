#include "textfield.hpp"
#include "gui.hpp"
#include <rvg/font.hpp>
#include <nytl/utf.hpp>
#include <dlg/dlg.hpp>

namespace vui {

Textfield::Textfield(Gui& gui, Vec2f pos) :
	Textfield(gui, {pos, {autoSize, autoSize}}) {
}

Textfield::Textfield(Gui& gui, const Rect2f& bounds) :
	Textfield(gui, bounds, gui.styles().textfield) {
}

Textfield::Textfield(Gui& gui, const Rect2f& bounds,
		const TextfieldStyle& style) : Widget(gui, bounds), style_(style) {

	auto font = style.font ? style.font : &gui.font();

	bg_ = {context()};
	selection_.bg = {context(), {}, {}, {true, 0.f}};
	selection_.bg.disable(true);

	text_ = {context(), U"", *font, {}};
	selection_.text = {context(), U"", *font, {}};
	selection_.text.disable(true);

	cursor_ = {context(), {}, {style.cursorWidth, font->height()}, {true, 0.f}};
	cursor_.disable(true);

	size(bounds.size);
}

void Textfield::size(Vec2f size) {
	auto tc = text_.change();
	auto textSize = Vec {text_.width(), gui().font().height()};
	tc->position = style().padding;

	auto bgc = bg_.change();
	bgc->size = size;
	bgc->rounding = style().rounding;
	bgc->drawMode = {true, style().bgStroke ? 2.f : 0.f};

	if(size.x == autoSize) {
		size.x = tc->font->width(U".:This is the default textfield length:.");
		bgc->size.x = size.x;
	}

	if(size.y != autoSize) {
		tc->position.y = (size.y - textSize.y) / 2;
	} else {
		size.y = style().padding.y * 2 + textSize.y;
		bgc->size.y = size.y;
	}

	updateSelectionDraw();
	updateCursorPosition();
	Widget::size(size);
}

void Textfield::hide(bool hide) {
	bg_.disable(hide);
	text_.disable(hide);
	cursor_.disable(hide);
	selection_.bg.disable(hide);
	selection_.text.disable(hide);
}

bool Textfield::hidden() const {
	return bg_.disabled();
}

Widget* Textfield::mouseButton(const MouseButtonEvent& ev) {
	if(ev.button != MouseButton::left) {
		return nullptr;
	}

	selecting_ = ev.pressed;
	if(ev.pressed) {
		if(selection_.count) {
			endSelection();
		}

		auto ex = ev.position.x - (position().x + text_.position().x);
		cursorPos_ = charAt(ex);

		cursor(true, true, false);
		updateCursorPosition();
		dlg_assert(cursorPos_ <= text_.utf32().length());
	} else if(!selection_.count) {
		cursor(true, true, true);
	}

	return this;
}

Widget* Textfield::mouseMove(const MouseMoveEvent& ev) {
	if(selecting_) {
		auto ps = selection_.start;
		auto pc = selection_.count;

		auto c1 = cursorPos_;
		auto c2 = charAt(ev.position.x - (position().x + text_.position().x));
		selection_.start = std::min(c1, c2);
		selection_.count = std::abs(int(c1) - int(c2));

		if(ps != selection_.start || pc != selection_.count) {
			gui().listener().selection(utf8Selected());
			updateSelectionDraw();
		}
	}

	return this;
}

void Textfield::updateSelectionDraw() {
	if(!selection_.count) {
		selection_.bg.disable(true);
		return;
	}

	cursor(false, true, false);
	auto end = selection_.start + selection_.count - 1;

	auto b1 = text_.ithBounds(selection_.start);
	auto b2 = text_.ithBounds(end);

	b1.position += text_.position();
	b2.position += text_.position();

	auto sc = selection_.bg.change();
	sc->position.x = b1.position.x;
	sc->position.y = text_.position().y - 1.f;
	sc->size.x = b2.position.x + b2.size.x - b1.position.x;
	sc->size.y = text_.font()->height() + 2.f;
	selection_.bg.disable(false);

	auto tc = selection_.text.change();
	tc->position.x = b1.position.x;
	tc->position.y = text_.position().y;
	tc->utf32 = {
		text_.utf32().begin() + selection_.start,
		text_.utf32().begin() + selection_.start + selection_.count};
	selection_.text.disable(false);
}

void Textfield::focus(bool gained) {
	if(!gained && selection_.count) {
		endSelection();
	}

	focus_ = gained;
	cursor(focus_);
}

Widget* Textfield::textInput(const TextInputEvent& ev) {
	auto utf32 = toUtf32(ev.utf8);

	{
		auto tc = text_.change();
		tc->utf32.insert(cursorPos_, utf32);
		dlg_assert(cursorPos_ <= tc->utf32.length());
	}

	cursorPos_ += utf32.length();
	updateCursorPosition();
	cursor(true, true);
	if(onChange) {
		onChange(*this);
	}

	return this;
}

Widget* Textfield::key(const KeyEvent& ev) {
	bool changed = false;
	bool updateCursor = false;
	if(ev.pressed) {
		if(ev.key == Key::backspace && cursorPos_ > 0) {
			auto tc = text_.change();
			changed = true;
			if(selection_.count) {
				tc->utf32.erase(selection_.start, selection_.count);
				cursorPos_ = selection_.start;
				endSelection();
			} else {
				cursorPos_ -= 1;
				tc->utf32.erase(cursorPos_, 1);
			}
			updateCursor = true;
		} else if(ev.key == Key::left) {
			if(selection_.count) {
				cursorPos_ = selection_.start;
				endSelection();
			} else if(cursorPos_ > 0) {
				cursorPos_ -= 1;
			}

			updateCursor = true;
			cursor(true, true);
		} else if(ev.key == Key::right) {
			if(selection_.count) {
				cursorPos_ = selection_.start + selection_.count;
				endSelection();
				updateCursor = true;
			} else if(cursorPos_ < text_.utf32().length()) {
				cursorPos_ += 1;
				updateCursor = true;
				cursor(true, true);
			}
		} else if(ev.key == Key::del) {
			auto tc = text_.change();
			if(selection_.count) {
				tc->utf32.erase(selection_.start, selection_.count);
				cursorPos_ = selection_.start;
				updateCursor = true;
				endSelection();
				changed = true;
			} else if(cursorPos_ < text_.utf32().length()) {
				tc->utf32.erase(cursorPos_, 1);
				changed = true;
			}
		}
	}

	if(updateCursor) {
		updateCursorPosition();
	}

	if(changed && onChange) {
		onChange(*this);
	}

	dlg_assert(cursorPos_ <= text_.utf32().length());
	return this;
}

void Textfield::draw(const DrawInstance& di) const {
	Widget::bindState(di);

	dlg_assert(style().bg);
	style().bg->bind(di);
	bg_.fill(di);

	if(style().bgStroke) {
		style().bgStroke->bind(di);
		bg_.stroke(di);
	}

	dlg_assert(style().selected);
	style().selected->bind(di);
	selection_.bg.fill(di);

	dlg_assert(style().text);
	style().text->bind(di);
	text_.draw(di);

	if(style().selectedText) {
		style().selectedText->bind(di);
		selection_.text.draw(di);
	}

	dlg_assert(style().cursor);
	style().cursor->bind(di);
	cursor_.fill(di);
}

void Textfield::update(double delta) {
	if(!focus_ || !blink_ || hidden()) {
		return;
	}

	constexpr auto blinkTime = 0.5;
	blinkAccum_ = blinkAccum_ + delta;
	if(blinkAccum_ > blinkTime) {
		blinkAccum_ = std::fmod(blinkAccum_, blinkTime);
		cursor_.disable(!cursor_.disabled());
	}

	registerUpdate();
}

void Textfield::updateCursorPosition() {
	dlg_assert(cursorPos_ <= text_.utf32().length());
	auto x = 0.f;

	if(cursorPos_ > 0) {
		auto b = text_.ithBounds(cursorPos_ - 1);
		x += b.position.x + b.size.x;
	}

	// scrolling
	auto xbeg = style().padding.x;
	auto xend = size().x - 2 * style().padding.x;

	auto tc = text_.change();
	auto abs = tc->position.x + x;
	auto clamped = std::clamp(abs, xbeg, xend);

	tc->position.x += clamped - abs;
	abs += clamped - abs;

	auto cc = cursor_.change();
	cc->position.x = abs;
	cc->position.y = tc->position.y;
	cc->drawMode = {true, false};
}

void Textfield::cursor(bool show, bool resetBlink, bool blink) {
	blink_ = blink;
	cursor_.disable(!show);

	if(resetBlink) {
		blinkAccum_ = 0.f;
	}

	if(blink_) {
		registerUpdate();
	}
}

unsigned Textfield::charAt(float x) {
	auto ca = text_.charAt(x);
	if(ca < text_.utf32().length()) {
		auto bounds = text_.ithBounds(ca);
		auto into = (x - bounds.position.x) / bounds.size.x;
		if(into >= 0.5f) {
			++ca;
		}
	}

	return ca;
}

std::u32string_view Textfield::utf32() const {
	return text_.utf32();
}

std::string Textfield::utf8() const {
	return text_.utf8();
}

std::u32string_view Textfield::utf32Selected() const {
	return {text_.utf32().data() + selection_.start, selection_.count};
}

std::string Textfield::utf8Selected() const {
	return toUtf8(utf32Selected());
}

void Textfield::endSelection() {
	selection_.count = selection_.start = {};
	selection_.text.disable(true);
	selection_.bg.disable(true);

	if(focus_) {
		cursor(true, true, false);
	}
}

} // namespace vui
