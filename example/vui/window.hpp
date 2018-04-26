#pragma once

#include "fwd.hpp"
#include "container.hpp"
#include "style.hpp"
#include <rvg/shapes.hpp>

namespace vui {

constexpr struct ResizeWidgetTag {} resizeWidget = {};

// TODO: add create<> method
class Pane : public Widget {
public:
	Pane(Gui& gui, const Rect2f& bounds, std::unique_ptr<Widget> = {});
	Pane(Gui& gui, const Rect2f& bounds, const PaneStyle& style,
		std::unique_ptr<Widget> = {});

	std::unique_ptr<Widget> widget(std::unique_ptr<Widget>,
		bool resizeSelf = false);
	auto* widget() const { return widget_.get(); }
	const auto& style() const { return style_.get(); }

	void position(Vec2f position) override;
	void size(Vec2f size) override;
	void size(ResizeWidgetTag, Vec2f size);
	using Widget::size;
	using Widget::position;

	void hide(bool hide) override;
	bool hidden() const override;
	void draw(const DrawInstance&) const override;
	void refreshTransform() override;

	Widget* mouseMove(const MouseMoveEvent&) override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseWheel(const MouseWheelEvent&) override;
	Widget* key(const KeyEvent&) override;
	Widget* textInput(const TextInputEvent&) override;

protected:
	std::reference_wrapper<const PaneStyle> style_;
	std::unique_ptr<Widget> widget_ {};
	bool mouseOver_ {};
	RectShape bg_;
};

class Window : public LayoutWidget {
public:
	Window(Gui& gui, const Rect2f& bounds);
	Window(Gui& gui, const Rect2f& bounds, const WindowStyle& style);

	/// Refreshes the layout of its widgets.
	/// Must be called every time a child widget is resized.
	void refreshLayout();

	void size(Vec2f size) override;
	using LayoutWidget::size;

	void hide(bool hide) override;
	bool hidden() const override;
	void draw(const DrawInstance&) const override;

	const auto& style() const { return style_.get(); }

protected:
	Vec2f nextSize() const override;
	Vec2f nextPosition() const override;

protected:
	std::reference_wrapper<const WindowStyle> style_;
	RectShape bg_;
};

} // namespace vui
