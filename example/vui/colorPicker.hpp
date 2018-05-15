#pragma once

#include "fwd.hpp"
#include "widget.hpp"
#include "style.hpp"
#include "button.hpp"

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
	void draw(vk::CommandBuffer) const override;

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

class ColorButton : public BasicButton {
public:
	std::function<void(ColorButton&)> onChange;

public:
	ColorButton(Gui&, const Rect2f& bounds,
		const Vec2f& pickerSize = {autoSize, autoSize},
		const Color& start = {20, 20, 20});
	ColorButton(Gui&, const Rect2f& bounds, const Vec2f& pickerSize,
		const Color& start, const ColorButtonStyle& style);

	using Widget::size;
	using Widget::position;

	void size(Vec2f size) override;
	void position(Vec2f pos) override;

	void hide(bool hide) override;

	void focus(bool gained) override;
	void draw(vk::CommandBuffer) const override;

	const auto& style() const { return style_.get(); }
	const auto& cp() const { return *cp_; }
	auto picked() const { return cp().picked(); }

protected:
	void clicked(const MouseButtonEvent&) override;

protected:
	std::reference_wrapper<const ColorButtonStyle> style_;

	Paint colorPaint_;
	RectShape color_;
	Pane* pane_;
	ColorPicker* cp_;
};

} // namespace vui
