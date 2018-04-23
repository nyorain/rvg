#pragma once

#include "fwd.hpp"
#include "widget.hpp"
#include "style.hpp"

#include <rvg/shapes.hpp>
#include <rvg/paint.hpp>
#include <rvg/text.hpp>

#include <functional>

namespace vui {

class ColorPicker : public Widget {
public:
	std::function<void(ColorPicker&)> onChange;

public:
	ColorPicker(Gui&, const Rect2f& bounds, const Color& start = {20, 20, 20});
	ColorPicker(Gui&, const Rect2f& bounds, const Color& start,
			const ColorPickerStyle& style);

	void size(Vec2f) override;
	using Widget::size;

	void hide(bool hide) override;
	bool hidden() const override;

	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseMove(const MouseMoveEvent&) override;
	void draw(const DrawInstance&) const override;

	void pick(const Color&);
	Color picked() const;

	float currentHue() const;
	Vec2f currentSV() const;

	const auto& style() const { return style_.get(); }

protected:
	Rect2f ownScissor() const override;
	void click(Vec2f pos, bool real);
	void size(Vec3f hsv, Vec2f size);

protected:
	std::reference_wrapper<const ColorPickerStyle> style_;

	Shape hue_;
	RectShape hueMarker_;

	RectShape selector_;
	CircleShape colorMarker_;

	Paint basePaint_ {};
	Paint sGrad_ {}; // saturation gradient
	Paint vGrad_ {}; // value gradient

	bool slidingSV_ {};
	bool slidingHue_ {};
};

class ColorButton : public Widget {
public:
	ColorButton(Gui&, const Rect2f& bounds,
		const Vec2f& pickerSize = {autoSize, autoSize},
		const Color& start = {20, 20, 20});
	ColorButton(Gui&, const Rect2f& bounds, const Vec2f& pickerSize,
		const Color& start, const ColorButtonStyle& style);

	void size(Vec2f size) override;
	using Widget::size;

	void position(Vec2f pos) override;
	using Widget::position;

	void hide(bool hide) override;
	bool hidden() const override;

	void mouseOver(bool gained) override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	void focus(bool gained) override;
	void draw(const DrawInstance&) const override;

	auto& colorPicker() const { return *cp_; }
	const auto& style() const { return style_.get(); }

protected:
	std::reference_wrapper<const ColorButtonStyle> style_;

	RectShape bg_;
	RectShape color_;
	ColorPicker* cp_ {};

	bool hovered_;
	bool pressed_;
};

} // namespace vui
