#pragma once

#include "fwd.hpp"
#include "widget.hpp"
#include "style.hpp"

#include <rvg/shapes.hpp>

namespace vui {

class Checkbox : public Widget {
public:
	std::function<void(Checkbox&)> onToggle;

public:
	Checkbox(Gui&, Vec2f pos);
	Checkbox(Gui&, const Rect2f& bounds);
	Checkbox(Gui&, const Rect2f& bounds, const CheckboxStyle& style);

	auto checked() const { return checked_; }
	void set(bool);
	void toggle() { set(!checked()); }

	void size(Vec2f size) override;
	using Widget::size;

	void hide(bool hide) override;
	bool hidden() const override;

	Widget* mouseButton(const MouseButtonEvent&) override;
	void draw(const DrawInstance&) const override;

	const auto& style() const { return style_.get(); }

protected:
	std::reference_wrapper<const CheckboxStyle> style_;
	rvg::RectShape bg_;
	rvg::RectShape fg_;
	bool checked_ {};
};

} // namespace vui

