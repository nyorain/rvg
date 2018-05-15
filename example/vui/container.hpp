#pragma once

#include "fwd.hpp"
#include "input.hpp"
#include "widget.hpp"

namespace vui {

/// Owns widgets and manages them, i.e. forwards input and rendering calls.
class WidgetContainer {
public:
	WidgetContainer(Gui& gui) : gui_(gui) {}

	Widget* mouseMove(const MouseMoveEvent&);
	Widget* mouseButton(const MouseButtonEvent&);
	Widget* mouseWheel(const MouseWheelEvent&);
	Widget* key(const KeyEvent&);
	Widget* textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	/// Just draws all owned widgets.
	void draw(vk::CommandBuffer) const;
	void refreshTransform();

	auto& gui() const { return gui_; }

	/// Returns the child widget with focus/over which the mouse hovers.
	/// Returns nullptr if there is no such child.
	Widget* childMouseOver() const { return mouseOver_; }
	Widget* childFocus() const { return focus_; }

protected:
	/// Adds the given widget to the container.
	/// Derived classes by adjust its position/size before adding it.
	virtual Widget& add(std::unique_ptr<Widget>);

	/// Creates a widget of the given type with the given arguments and
	/// adds it to this container. The widget must have a
	/// constructor(Gui&, Args&&...).
	/// Returns a reference to the created widget.
	template<typename W, typename... Args>
	W& create(Args&&... args) {
		static_assert(std::is_base_of_v<Widget, W>, "Can only create widgets");
		auto widget = std::make_unique<W>(gui(), std::forward<Args>(args)...);
		auto& ret = *widget;
		add(std::move(widget));
		return ret;
	}

protected:
	Gui& gui_;
	std::vector<std::unique_ptr<Widget>> widgets_;
	Widget* focus_ {};
	Widget* mouseOver_ {};
};

/// Widget that contains other widgets.
class ContainerWidget : public Widget, public WidgetContainer {
public:
	ContainerWidget(Gui&, const Rect2f& bounds);

	void hide(bool hide) override = 0;
	void size(Vec2f) override = 0;
	using Widget::size;

	void position(Vec2f) override;
	using Widget::position;

	void refreshTransform() override;

	Widget* mouseMove(const MouseMoveEvent&) override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseWheel(const MouseWheelEvent&) override;
	Widget* key(const KeyEvent&) override;
	Widget* textInput(const TextInputEvent&) override;

	void focus(bool) override;
	void mouseOver(bool) override;

	void draw(vk::CommandBuffer) const override;

	using Widget::gui;

protected:
	Widget& add(std::unique_ptr<Widget>) override;
};

/// ContainerWidget that exposes functionality for creating new widgets.
class LayoutWidget : public ContainerWidget {
public:
	using ContainerWidget::ContainerWidget;

	/// Creates a widget with the given arguments and adds it to the window.
	/// Will automatically position and size it, see createSized if you
	/// explicitly want to specify the size.
	/// The Widget must be constructible with (Gui&, Rect2f, Args...) as
	/// arguments.
	/// Returns a reference to it.
	template<typename W, typename... Args>
	W& create(Args&&... args) {
		return createSized<W>(nextSize(), std::forward<Args>(args)...);
	}

	/// Like create but lets the caller explicitly specify the size passed
	/// to the widget.
	template<typename W, typename... Args>
	W& createSized(Vec2f size, Args&&... args) {
		return ContainerWidget::create<W>(Rect2f {nextPosition(), size},
			std::forward<Args>(args)...);
	}

protected:
	virtual Vec2f nextSize() const = 0;
	virtual Vec2f nextPosition() const = 0;
};

} // namespace vui
