#include "window.hpp"
#include "gui.hpp"

namespace vui {

Window::Window(Gui& gui, const Rect2f& bounds) :
	Window(gui, bounds, gui.styles().window) {
}

Window::Window(Gui& gui, const Rect2f& bounds, const WindowStyle& style) :
		LayoutWidget(gui, bounds), style_(style) {

	auto stroke = style.bgStroke ? 2.f : 0.f;
	auto drawMode = DrawMode {true, stroke};
	drawMode.aaFill = true;
	bg_ = {context(), {}, size(), drawMode, style.rounding};
}

void Window::refreshLayout() {
	auto pos = style().outerPadding;
	for(auto& w : widgets_) {
		w->position(pos);
		pos.y += w->size().y;
		pos.y += style().innerPadding;
	}
}

void Window::size(Vec2f size) {
	bg_.change()->size = size;
	LayoutWidget::size(size);
}

void Window::hide(bool hide) {
	LayoutWidget::hide(hide);
	bg_.disable(hide);
}

bool Window::hidden() const {
	return bg_.disabled();
}

void Window::draw(const DrawInstance& di) const {
	Widget::bindState(di);

	if(style().bg) {
		style().bg->bind(di);
		bg_.fill(di);
	}

	if(style().bgStroke) {
		style().bgStroke->bind(di);
		bg_.stroke(di);
	}

	LayoutWidget::draw(di);
}

Vec2f Window::nextSize() const {
	return {autoSize, autoSize};
}

Vec2f Window::nextPosition() const {
	auto pos = position() + style().outerPadding;
	if(!widgets_.empty()) {
		pos = widgets_.back()->position();
		pos.y += widgets_.back()->size().y;
		pos.y += style().innerPadding;
	}

	return pos;
}

} // namespace vui
