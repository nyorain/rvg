#include "checkbox.hpp"
#include "gui.hpp"

namespace vui {

Checkbox::Checkbox(Gui& gui, Vec2f pos) :
	Checkbox(gui, {pos, {autoSize, autoSize}}) {
}

Checkbox::Checkbox(Gui& gui, const Rect2f& bounds) :
	Checkbox(gui, bounds, gui.styles().checkbox) {
}

Checkbox::Checkbox(Gui& gui, const Rect2f& bounds, const CheckboxStyle& style) :
		Widget(gui, bounds), style_(style) {
	auto stroke = style.bgStroke ? 2.f : 0.f;
	bg_ = {context(), {}, {}, {true, stroke}};
	fg_ = {context(), style.padding, {}, {true, 0.f}};
	fg_.disable(true);
	size(bounds.size);
}

void Checkbox::set(bool checked) {
	if(checked == checked_) {
		return;
	}

	checked_ = checked;
	fg_.disable(!checked);
}

void Checkbox::size(Vec2f size) {
	if(size.x == autoSize && size.y == autoSize) {
		size = {15.f, 15.f};
	} else if(size.x == autoSize) {
		size.x = size.y;
	} else if(size.y == autoSize) {
		size.y = size.x;
	}

	using namespace nytl::vec::cw;
	bg_.change()->size = size;
	fg_.change()->size = max(size - 2 * style().padding, nytl::Vec {0.f, 0.f});
	Widget::size(size);
}

void Checkbox::hide(bool hide) {
	bg_.disable(hide);
	if(hide) {
		fg_.disable(true);
	} else if(checked_) {
		fg_.disable(false);
	}
}

bool Checkbox::hidden() const {
	return bg_.disabled();
}

Widget* Checkbox::mouseButton(const MouseButtonEvent& ev) {
	if(ev.button == MouseButton::left && ev.pressed) {
		toggle();
		if(onToggle) {
			onToggle(*this);
		}
	}

	return this;
}

void Checkbox::draw(vk::CommandBuffer cb) const {
	Widget::bindState(cb);

	style().bg->bind(cb);
	bg_.fill(cb);

	if(style().bgStroke) {
		style().bgStroke->bind(cb);
		bg_.stroke(cb);
	}

	style().fg->bind(cb);
	fg_.fill(cb);
}

} // namespace vui
