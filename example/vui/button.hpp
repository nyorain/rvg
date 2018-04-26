#pragma once

#include "fwd.hpp"
#include "widget.hpp"
#include "style.hpp"

#include <rvg/shapes.hpp>
#include <rvg/text.hpp>

#include <functional>
#include <string_view>

namespace vui {

class BasicButton : public Widget {
public:
	BasicButton(Gui&, Vec2f pos);
	BasicButton(Gui&, const Rect2f& bounds, const BasicButtonStyle&);

	/// Sets/updates the hint for this button.
	/// When an empty string view is passed, the hint
	/// will be disabled.
	void hint(std::string_view hint);

	void size(Vec2f size) override;
	using Widget::size;

	void hide(bool hide) override;
	bool hidden() const override;

	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseMove(const MouseMoveEvent&) override;
	void mouseOver(bool gained) override;
	void draw(const DrawInstance&) const override;

	const auto& style() const { return style_.get(); }
	bool hovered() const { return hovered_; }
	bool pressed() const { return pressed_; }

protected:
	virtual void updatePaints();
	virtual void clicked(const MouseButtonEvent&) {}

protected:
	std::reference_wrapper<const BasicButtonStyle> style_;
	RectShape bg_;
	Paint bgFill_;
	Paint bgStroke_;
	bool hovered_ {};
	bool pressed_ {};
	DelayedHint* hint_ {};
};

class LabeledButton : public BasicButton {
public:
	std::function<void(LabeledButton&)> onClick;

public:
	LabeledButton(Gui&, Vec2f pos, std::string_view label);
	LabeledButton(Gui&, const Rect2f& bounds, std::string_view label);
	LabeledButton(Gui&, const Rect2f& bounds, std::string_view label,
		const LabeledButtonStyle&);

	void size(Vec2f size) override;
	using Widget::size;

	void hide(bool hide) override;
	void draw(const DrawInstance&) const override;

	const auto& style() const { return style_.get(); }

protected:
	virtual void clicked(const MouseButtonEvent&) override;

protected:
	std::reference_wrapper<const LabeledButtonStyle> style_;

	Text label_;
	RectShape bg_;
	Paint bgFill_;
	Paint bgStroke_;
};

} // namespace vui

