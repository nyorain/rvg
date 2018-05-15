#include "button.hpp"
#include "gui.hpp"
#include "hint.hpp"
#include <rvg/font.hpp>
#include <dlg/dlg.hpp>

namespace vui {

// Basicbutton
BasicButton::BasicButton(Gui& gui, Vec2f pos) :
	BasicButton(gui, {pos, {autoSize, autoSize}}, gui.styles().basicButton) {
}

BasicButton::BasicButton(Gui& gui, const Rect2f& bounds,
		const BasicButtonStyle& style) : Widget(gui, bounds), style_(style) {

	bool stroke = false;
	for(auto& draw : {style.hovered, style.normal, style.pressed}) {
		if(draw.bgStroke) {
			stroke = true;
			break;
		}
	}

	bg_ = {context(), {}, {}, {true, stroke ? 2.f : 0.f}, style.rounding};
	bgFill_ = {context(), {}};
	if(stroke) {
		bgStroke_ = {context(), {}};
	}

	updatePaints();
	size(bounds.size);
}

void BasicButton::hint(std::string_view text) {
	if(text.empty()) {
		hint_ = {};
	} else {
		hint_ = &gui().create<DelayedHint>(position(), text);
	}
}

void BasicButton::updatePaints() {
	auto& draw =
		pressed_ ? style().pressed :
		hovered_ ? style().hovered : style().normal;
	*bgFill_.change() = draw.bg;
	if(draw.bgStroke.has_value()) {
		dlg_assert(bgStroke_.valid());
		*bgStroke_.change() = *draw.bgStroke;
	} else if(bgStroke_.valid()) {
		bg_.disable(!draw.bgStroke.has_value(), DrawType::stroke);
	}
}

Widget* BasicButton::mouseButton(const MouseButtonEvent& event) {
	if(event.button != MouseButton::left) {
		return nullptr;
	}

	if(event.pressed) {
		pressed_ = true;
		updatePaints();
	} else if(pressed_) {
		pressed_ = false;
		updatePaints();
		if(hovered_) {
			clicked(event);
		}
	}

	return this;
}

void BasicButton::size(Vec2f size) {
	if(size.x == autoSize) {
		size.x = 130;
	}

	if(size.y == autoSize) {
		size.y = 30;
	}

	bg_.change()->size = size;
	Widget::size(size);
}

void BasicButton::hide(bool hide) {
	bg_.disable(hide);
}

bool BasicButton::hidden() const {
	return bg_.disabled();
}

Widget* BasicButton::mouseMove(const MouseMoveEvent& ev) {
	if(this->contains(ev.position) && hint_) {
		hint_->hovered(true);
		hint_->position(ev.position + gui().hintOffset);
	} else if(hint_) {
		hint_->hovered(false);
	}

	return this;
}

void BasicButton::mouseOver(bool gained) {
	hovered_ = gained;
	if(hint_) {
		hint_->hovered(hovered_);
	}
	updatePaints();
}

void BasicButton::draw(vk::CommandBuffer cb) const {
	bindState(cb);

	bgFill_.bind(cb);
	bg_.fill(cb);

	if(bgStroke_.valid()) {
		bgStroke_.bind(cb);
		bg_.stroke(cb);
	}
}

// Button
LabeledButton::LabeledButton(Gui& gui, Vec2f pos, std::string_view text) :
	LabeledButton(gui, {pos, {autoSize, autoSize}}, text) {
}

LabeledButton::LabeledButton(Gui& gui, const Rect2f& bounds,
	std::string_view text) :
		LabeledButton(gui, bounds, text, gui.styles().labeledButton) {
}

LabeledButton::LabeledButton(Gui& gui, const Rect2f& bounds,
	std::string_view text, const LabeledButtonStyle& style) :
		BasicButton(gui, bounds, style.basic ?
			*style.basic : gui.styles().basicButton), style_(style) {

	auto& font = style.font ? *style.font : gui.font();
	label_ = {context(), text, font, {}};
	this->size(bounds.size);
}

void LabeledButton::clicked(const MouseButtonEvent&) {
	if(onClick) {
		onClick(*this);
	}
}

void LabeledButton::size(Vec2f size) {
	auto tc = label_.change();
	auto textSize = Vec {label_.width(), label_.font()->height()};
	tc->position = style().padding;

	if(size.x != autoSize) {
		tc->position.x = (size.x - textSize.x) / 2;
	} else {
		size.x = textSize.x + 2 * style().padding.x;
	}

	if(size.y != autoSize) {
		tc->position.y = (size.y - textSize.y) / 2;
	} else {
		size.y = textSize.y + 2 * style().padding.y;
	}

	// resizes scissor
	BasicButton::size(size);
}

void LabeledButton::hide(bool hide) {
	BasicButton::hide(hide);
	label_.disable(hide);
}

void LabeledButton::draw(vk::CommandBuffer cb) const {
	BasicButton::draw(cb);
	style().label->bind(cb);
	label_.draw(cb);
}

} // namespace vui
