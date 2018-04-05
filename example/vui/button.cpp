#include "button.hpp"
#include "gui.hpp"
#include "hint.hpp"
#include <rvg/font.hpp>
#include <dlg/dlg.hpp>

namespace vui {

Button::Button(Gui& gui, Vec2f pos, std::string_view text) :
	Button(gui, {pos, {autoSize, autoSize}}, text) {
}

Button::Button(Gui& gui, const Rect2f& bounds, std::string_view text) :
	Button(gui, bounds, text, gui.styles().button) {
}

Button::Button(Gui& gui, const Rect2f& bounds, std::string_view text,
		const ButtonStyle& style) : Widget(gui, bounds), style_(style) {

	auto font = style.font ? style.font : &gui.font();
	bg_ = {context()};
	label_ = {context(), text, *font, {}};
	hint_ = &gui.create<DelayedHint>(Vec2f {}, "Button Hint");

	bgFill_ = {context(), {}};
	bgStroke_ = {context(), {}};
	labelPaint_ = {context(), {}};

	updatePaints();
	size(bounds.size);
}

void Button::updatePaints() {
	auto& draw =
		pressed_ ? style().pressed :
		hovered_ ? style().hovered : style().normal;
	*labelPaint_.change() = draw.label;
	*bgFill_.change() = draw.bg;

	bg_.disable(!draw.bgStroke.has_value(), DrawType::stroke);
	if(draw.bgStroke.has_value()) {
		*bgStroke_.change() = *draw.bgStroke;
	}
}

Widget* Button::mouseButton(const MouseButtonEvent& event) {
	if(event.button != MouseButton::left) {
		return nullptr;
	}

	if(event.pressed) {
		pressed_ = true;
		updatePaints();
	} else if(pressed_) {
		pressed_ = false;
		updatePaints();
		if(hovered_ && onClick) {
			onClick(*this);
		}
	}

	return this;
}

void Button::size(Vec2f size) {
	auto tc = label_.change();
	auto textSize = Vec {label_.width(), gui().font().height()};
	tc->position = style().padding;

	auto bgc = bg_.change();
	bgc->size = size;
	bgc->rounding = style().rounding;
	bgc->drawMode = {true, 2.f};

	if(size.x != autoSize) {
		tc->position.x = (size.x - textSize.x) / 2;
	} else {
		size.x = textSize.x + 2 * style().padding.x;
		bgc->size.x = size.x;
	}

	if(size.y != autoSize) {
		tc->position.y = (size.y - textSize.y) / 2;
	} else {
		size.y = textSize.y + 2 * style().padding.y;
		bgc->size.y = size.y;
	}

	// resizes scissor
	Widget::size(size);
}

void Button::hide(bool hide) {
	bg_.disable(hide);
	label_.disable(hide);
}

bool Button::hidden() const {
	return bg_.disabled();
}

Widget* Button::mouseMove(const MouseMoveEvent& ev) {
	if(this->contains(ev.position)) {
		hint_->hovered(true);
		hint_->position(ev.position + gui().hintOffset);
	} else {
		hint_->hovered(false);
	}

	return this;
}

void Button::mouseOver(bool gained) {
	hovered_ = gained;
	hint_->hovered(hovered_);
	updatePaints();
}

void Button::draw(const DrawInstance& di) const {
	bindState(di);

	bgFill_.bind(di);
	bg_.fill(di);

	labelPaint_.bind(di);
	label_.draw(di);

	bgStroke_.bind(di);
	bg_.stroke(di);
}

} // namespace vui
