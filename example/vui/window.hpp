#pragma once

#include "fwd.hpp"
#include "container.hpp"
#include "style.hpp"

namespace vui {

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
