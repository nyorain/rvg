#include "window.hpp"
#include "gui.hpp"
#include <rvg/context.hpp>
#include <dlg/dlg.hpp>

namespace vui {

// Pane
Pane::Pane(Gui& gui, const Rect2f& bounds, std::unique_ptr<Widget> widget) :
	Pane(gui, bounds, gui.styles().pane, std::move(widget)) {
}

Pane::Pane(Gui& gui, const Rect2f& bounds, const PaneStyle& style,
	std::unique_ptr<Widget> widget) :
		Widget(gui, bounds), style_(style), widget_(std::move(widget)) {

	auto stroke = style.bgStroke ? 2.f : 0.f;
	bg_ = {context(), {}, {}, {true, stroke}, style.rounding};

	this->size(bounds.size);
	if(widget_) {
		widget_->position(position() + style.padding);
	}
}

std::unique_ptr<Widget> Pane::widget(std::unique_ptr<Widget> widget,
		bool resizeSelf) {
	std::swap(widget, widget_);
	dlg_assert(widget_);

	auto s  = widget->size();
	if(resizeSelf) {
		s += 2 * style().padding;
		bg_.change()->size = s;
		Widget::size(s);
	} else if(s != size()) {
		widget_->size(size() - 2 * style().padding);
	}

	widget_->position(position() + style().padding);
	widget_->intersectScissor({position(), size()});

	context().rerecord();
	return widget;
}

void Pane::position(Vec2f pos) {
	Widget::position(pos);
	if(widget_) {
		widget_->position(pos + style().padding);
		widget_->intersectScissor({pos, size()});
	}
}

void Pane::size(Vec2f size) {
	auto widgetSize = widget_ ? widget_->size() : Vec2f{100, 100};
	if(size.x == autoSize) {
		size.x = widgetSize.x + 2 * style().padding.x;
	} else if(widget_) {
		widgetSize.x = std::max(0.f, size.x - 2 * style().padding.x);
	}

	if(size.y == autoSize) {
		size.y = widgetSize.y + 2 * style().padding.y;
	} else if(widget_) {
		widgetSize.y = std::max(0.f, size.y - 2 * style().padding.y);
	}

	if(widget_) {
		widget_->size(widgetSize);
		widget_->intersectScissor({position(), size});
	}

	bg_.change()->size = size;
	Widget::size(size);
}

void Pane::size(ResizeWidgetTag, Vec2f size) {
	this->size(size);
}

void Pane::hide(bool hide) {
	bg_.disable(hide);
	if(widget_) {
		widget_->hide(hide);
	}
}

bool Pane::hidden() const {
	return bg_.disabled();
}

void Pane::refreshTransform() {
	if(widget_) {
		widget_->refreshTransform();
	}

	Widget::refreshTransform();
}

void Pane::draw(vk::CommandBuffer cb) const {
	Widget::bindState(cb);

	style().bg->bind(cb);
	bg_.fill(cb);

	if(style().bgStroke) {
		style().bgStroke->bind(cb);
		bg_.stroke(cb);
	}

	if(widget_) {
		widget_->draw(cb);
	}
}

Widget* Pane::mouseMove(const MouseMoveEvent& ev) {
	if(widget_) {
		auto contains = widget_->contains(ev.position);
		if(!mouseOver_ && contains) {
			mouseOver_ = true;
			widget_->mouseOver(true);
		} else if(mouseOver_ && !contains) {
			mouseOver_ = false;
			widget_->mouseOver(false);
			return widget_.get();
		} else if(mouseOver_) {
			return widget_->mouseMove(ev);
		}
	}

	return nullptr;
}

Widget* Pane::mouseButton(const MouseButtonEvent& ev) {
	if(mouseOver_ && widget_) {
		return widget_->mouseButton(ev);
	}

	return nullptr;
}

Widget* Pane::mouseWheel(const MouseWheelEvent& ev) {
	if(mouseOver_ && widget_) {
		return widget_->mouseWheel(ev);
	}

	return nullptr;
}

Widget* Pane::key(const KeyEvent& ev) {
	if(widget_) {
		return widget_->key(ev);
	}

	return nullptr;
}

Widget* Pane::textInput(const TextInputEvent& ev) {
	if(widget_) {
		return widget_->textInput(ev);
	}

	return nullptr;
}

// Window
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

void Window::draw(vk::CommandBuffer cb) const {
	Widget::bindState(cb);

	if(style().bg) {
		style().bg->bind(cb);
		bg_.fill(cb);
	}

	if(style().bgStroke) {
		style().bgStroke->bind(cb);
		bg_.stroke(cb);
	}

	LayoutWidget::draw(cb);
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
