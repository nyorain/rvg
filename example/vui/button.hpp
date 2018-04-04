#pragma once

#include "fwd.hpp"
#include "widget.hpp"
#include "style.hpp"

#include <rvg/shapes.hpp>
#include <rvg/text.hpp>

#include <functional>
#include <string_view>

namespace vui {

/// Small popup hint that displays text and processes no input.
class Button : public Widget {
public:
	std::function<void(Button&)> onClick;

public:
	Button(Gui&, Vec2f pos, std::string_view label);
	Button(Gui&, const Rect2f& bounds, std::string_view label);
	Button(Gui&, const Rect2f& bounds, std::string_view label,
		const ButtonStyle&);

	void size(Vec2f size) override;
	using Widget::size;

	void hide(bool hide) override;
	bool hidden() const override;

	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseMove(const MouseMoveEvent&) override;
	void mouseOver(bool gained) override;
	void draw(const DrawInstance&) const override;

	const auto& style() const { return style_.get(); }

protected:
	void updatePaints();

protected:
	std::reference_wrapper<const ButtonStyle> style_;

	Text label_;
	Paint labelPaint_;
	RectShape bg_;
	Paint bgFill_;
	Paint bgStroke_;

	bool hovered_ {};
	bool pressed_ {};

	DelayedHint* hint_;
};

} // namespace vui

