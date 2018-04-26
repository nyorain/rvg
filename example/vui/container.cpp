#include "container.hpp"
#include <dlg/dlg.hpp>

namespace vui {

// WidgetContainer
Widget* WidgetContainer::mouseMove(const MouseMoveEvent& ev) {
	for(auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
		auto& w = *it;
		if(!w->hidden() && w->contains(ev.position)) {
			if(w.get() != mouseOver_) {
				if(mouseOver_) {
					mouseOver_->mouseOver(false);
				}

				mouseOver_ = w.get();
				w->mouseOver(true);
			}

			return w->mouseMove(ev);
		}
	}

	if(mouseOver_) {
		mouseOver_->mouseOver(false);
		mouseOver_ = nullptr;
	}

	return nullptr;
}

Widget* WidgetContainer::mouseButton(const MouseButtonEvent& ev) {
	if(mouseOver_ != focus_) {
		if(focus_) {
			focus_->focus(false);
		}

		focus_ = mouseOver_;
		if(focus_) {
			focus_->focus(true);
		}
	}

	if(mouseOver_) {
		return mouseOver_->mouseButton(ev);
	}

	return nullptr;
}
Widget* WidgetContainer::mouseWheel(const MouseWheelEvent& ev) {
	if(mouseOver_) {
		return mouseOver_->mouseWheel(ev);
	}

	return nullptr;
}
Widget* WidgetContainer::key(const KeyEvent& ev) {
	if(focus_) {
		return focus_->key(ev);
	}

	return nullptr;
}
Widget* WidgetContainer::textInput(const TextInputEvent& ev) {
	if(focus_) {
		return focus_->textInput(ev);
	}

	return nullptr;
}
void WidgetContainer::focus(bool gained) {
	if(!gained && focus_) {
		focus_->focus(false);
		focus_ = nullptr;
	}
}
void WidgetContainer::mouseOver(bool gained) {
	if(!gained && mouseOver_) {
		mouseOver_->mouseOver(false);
		mouseOver_ = nullptr;
	}
}

void WidgetContainer::draw(const DrawInstance& di) const {
	for(auto& widget : widgets_) {
		dlg_assert(widget);
		widget->draw(di);
	}
}

Widget& WidgetContainer::add(std::unique_ptr<Widget> widget) {
	dlg_assert(widget);
	auto ub = std::upper_bound(widgets_.begin(), widgets_.end(),
		widget, [&](auto& a, auto& b){ return a->zOrder() < b->zOrder(); });
	widgets_.insert(ub, std::move(widget));
	return *widgets_.back();
}

void WidgetContainer::refreshTransform() {
	for(auto& w : widgets_) {
		dlg_assert(w);
		w->refreshTransform();
	}
}

// ContainerWidget
ContainerWidget::ContainerWidget(Gui& gui, const Rect2f& bounds)
	: Widget(gui, bounds), WidgetContainer(gui) {
}

void ContainerWidget::hide(bool hide) {
	for(auto& w : widgets_) {
		dlg_assert(w);
		w->hide(hide);
	}
}

void ContainerWidget::position(Vec2f pos) {
	for(auto& w : widgets_) {
		dlg_assert(w);
		auto rel = w->position() - position();
		w->position(pos + rel);
		w->intersectScissor({pos, size()});
	}

	Widget::position(pos);
}

void ContainerWidget::size(Vec2f size) {
	Widget::size(size);
	for(auto& w : widgets_) {
		dlg_assert(w);
		w->intersectScissor(bounds_);
	}
}

void ContainerWidget::refreshTransform() {
	Widget::refreshTransform();
	for(auto& w : widgets_) {
		dlg_assert(w);
		w->refreshTransform();
	}
}

Widget& ContainerWidget::add(std::unique_ptr<Widget> w) {
	w->intersectScissor(bounds());
	return WidgetContainer::add(std::move(w));
}

Widget* ContainerWidget::mouseMove(const MouseMoveEvent& ev) {
	return WidgetContainer::mouseMove(ev);
}

Widget* ContainerWidget::mouseButton(const MouseButtonEvent& ev) {
	return WidgetContainer::mouseButton(ev);
}

Widget* ContainerWidget::mouseWheel(const MouseWheelEvent& ev) {
	return WidgetContainer::mouseWheel(ev);
}

Widget* ContainerWidget::key(const KeyEvent& ev) {
	return WidgetContainer::key(ev);
}

Widget* ContainerWidget::textInput(const TextInputEvent& ev) {
	return WidgetContainer::textInput(ev);
}

void ContainerWidget::focus(bool gained) {
	return WidgetContainer::focus(gained);
}

void ContainerWidget::mouseOver(bool gained) {
	return WidgetContainer::mouseOver(gained);
}

void ContainerWidget::draw(const DrawInstance& di) const {
	WidgetContainer::draw(di);
}

} // namespace vui
