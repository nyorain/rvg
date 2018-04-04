#include "hint.hpp"
#include "gui.hpp"
#include <rvg/font.hpp>

namespace vui {

Hint::Hint(Gui& gui, Vec2f pos, std::string_view text) :
	Hint(gui, {pos, {autoSize, autoSize}}, text) {
}

Hint::Hint(Gui& gui, const Rect2f& bounds, std::string_view text) :
	Hint(gui, bounds, text, gui.styles().hint) {
}

Hint::Hint(Gui& gui, const Rect2f& bounds, std::string_view text,
		const HintStyle& style) : Widget(gui, bounds), style_(style) {

	auto font = style.font ? style.font : &gui.font();

	bg_ = {context()};
	bg_.disable(true);
	text_ = {context(), text, *font, {}};
	text_.disable(true);
	size(bounds.size);
}

void Hint::draw(const DrawInstance& di) const {
	bindState(di);

	if(style().bg) {
		style().bg->bind(di);
		bg_.fill(di);
	}

	if(style().bgStroke) {
		style().bgStroke->bind(di);
		bg_.stroke(di);
	}

	if(style().text) {
		style().text->bind(di);
		text_.draw(di);
	}
}

void Hint::hide(bool hide) {
	bg_.disable(hide);
	text_.disable(hide);
}

bool Hint::hidden() const {
	return bg_.disabled();
}

void Hint::size(Vec2f size) {
	auto textSize = Vec {text_.width(), text_.font()->height()};
	auto tc = text_.change();
	tc->position = style().padding;

	auto stroke = style().bgStroke ? 2.f : 0.f;
	auto bgc = bg_.change();
	bgc->drawMode = {true, stroke};
	bgc->size = size;
	bgc->rounding = style().rounding;

	if(size.x != autoSize) {
		tc->position.x = (size.x - textSize.x) / 2;
	} else {
		bgc->size.x = textSize.x + 2 * style().padding.x;
	}

	if(size.y != autoSize) {
		tc->position.y = (size.y - textSize.y) / 2;
	} else {
		bgc->size.y = textSize.y + 2 * style().padding.y;
	}

	// resizes scissor
	Widget::size(bgc->size);
}

// DelayedHint
void DelayedHint::hovered(bool hovered) {
	if(hovered && !hovered_) {
		accum_ = 0;
		hovered_ = true;
		registerUpdate();
	} else if(!hovered) {
		accum_ = 0;
		hide(true);
	}

	hovered_ = hovered;
}

void DelayedHint::update(double delta) {
	if(!hovered_) {
		return;
	}

	accum_ += delta;
	if(accum_ >= gui().hintDelay) {
		hide(false);
	} else {
		registerUpdate();
	}
}

} // namespace vui
