#pragma once

#include <vgv/vgv.hpp>
#include <nytl/vec.hpp>
#include <nytl/rect.hpp>
#include <nytl/callback.hpp>
#include <nytl/stringParam.hpp>
#include <unordered_set>

namespace vui {

using namespace nytl;
using namespace vgv;

constexpr float autoSize = -1.f;

enum class MouseButton {
	left = 2,
	right,
	middle,
	custom1,
	custom2
};

enum class Key {
	escape = 1,
	backspace = 14,
	enter = 28,
	up = 103,
	left = 105,
	right = 106,
	del = 111,
	down = 108,
};

// events
struct MouseMoveEvent {
	Vec2f position;
};

struct MouseButtonEvent {
	bool pressed;
	MouseButton button;
	Vec2f position;
};

struct MouseWheelEvent {
	float distance;
};

struct KeyEvent {
	Key key;
	bool pressed;
};

struct TextInputEvent {
	const char* utf8;
};

// gui
class Gui;
class Widget;

class GuiListener {
public:
	static GuiListener& nop();

public:
	virtual void copy(StringParam) {}
	virtual void pasteRequest(Widget&) {}
	virtual void selection(StringParam) {}
	virtual void focus(Widget*, Widget*) {}
	virtual void mouseOver(Widget*, Widget*) {}
};

class WidgetContainer {
public:
	WidgetContainer(Gui& gui) : gui_(gui) {}

	void mouseMove(const MouseMoveEvent&);
	bool mouseButton(const MouseButtonEvent&);
	void mouseWheel(const MouseWheelEvent&);
	bool key(const KeyEvent&);
	bool textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	void draw(const DrawInstance&) const;

	auto& gui() const { return gui_; }

	Widget* nextMouseOver() const { return mouseOver_; }
	Widget* nextFocus() const { return focus_; }

	Widget* mouseOver() const;
	Widget* focus() const;

	virtual Widget& add(std::unique_ptr<Widget>);
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

class Gui : public WidgetContainer {
public:
	struct ButtonDraw {
		vgv::PaintData label;
		vgv::PaintData bg;
	};

	struct ButtonStyle {
		ButtonDraw normal;
		ButtonDraw hovered;
		ButtonDraw pressed;
	};

	struct TextfieldStyle {
		vgv::PaintData label;
		vgv::PaintData bg;
	};

	struct WindowStyle {
		vgv::PaintData bg;
		vgv::PaintData border;
	};

	struct SliderStyle {
		vgv::PaintData left;
		vgv::PaintData right;
	};

	struct Styles {
		ButtonStyle button;
		TextfieldStyle textfield;
		WindowStyle window;
		SliderStyle slider;
	} styles;

public:
	Gui(Context& context, const Font& font, Styles&& s,
		GuiListener& listener = GuiListener::nop());

	void mouseMove(const MouseMoveEvent&);
	bool mouseButton(const MouseButtonEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	void update(double delta);
	bool updateDevice();

	Context& context() const { return context_; }
	const Font& font() const { return font_; }

	/// Registers the widget for the next update/updateDevice calls.
	void addUpdate(Widget&);
	void addUpdateDevice(Widget&);

	GuiListener& listener() { return listener_.get(); }
	void rerecord() { rerecord_ = true; }

protected:
	Context& context_;
	const Font& font_;
	std::reference_wrapper<GuiListener> listener_;
	std::unordered_set<Widget*> update_;
	std::unordered_set<Widget*> updateDevice_;
	std::pair<Widget*, MouseButton> buttonGrab_ {};
	bool rerecord_ {};
};

class Widget : public nytl::NonMovable {
public:
	Gui& gui;

public:
	Widget(Gui& xgui) : gui(xgui) {}
	Widget(Gui& xgui, Rect2f bounds) : gui(xgui), bounds_{bounds} {}
	virtual ~Widget() = default;

	virtual void bounds(const Rect2f&) = 0;

	virtual void mouseMove(const MouseMoveEvent&) {}
	virtual void mouseButton(const MouseButtonEvent&) {}
	virtual void mouseWheel(const MouseWheelEvent&) {}
	virtual void key(const KeyEvent&) {}
	virtual void textInput(const TextInputEvent&) {}

	virtual void focus(bool) {}
	virtual void mouseOver(bool) {}

	virtual void update(double) {}
	virtual bool updateDevice() { return false; }
	virtual void draw(const DrawInstance&) const {}

	virtual Widget* mouseOver() const { return nullptr; }
	virtual Widget* focus() const { return nullptr; }

	virtual bool contains(Vec2f point) const;
	virtual void position(Vec2f pos);
	virtual void size(Vec2f pos);

	const Rect2f& bounds() const { return bounds_; }
	Vec2f position() const { return bounds_.position; }
	Vec2f size() const { return bounds_.size; }

	void registerUpdate();
	void registerUpdateDevice();

protected:
	Rect2f bounds_ {};
};

class Button : public Widget {
public:
	Callback<void(Button&)> onClicked;

public:
	Button(Gui&, Vec2f pos, std::string xlabel);

	void bounds(const Rect2f& bounds) override;
	using Widget::bounds;

	void mouseButton(const MouseButtonEvent&) override;
	void mouseOver(bool gained) override;

	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
	static constexpr auto padding = Vec {40.f, 15.f};

	struct {
		struct {
			RectShape shape;
			Paint paint;
		} bg;

		struct {
			Text text;
			Paint paint;
		} label;
	} draw_;

	bool hovered_ {};
	bool pressed_ {};
};

class Textfield : public Widget {
public:
	Textfield(Gui&, Vec2f pos, float width);

	void bounds(const Rect2f& bounds) override;
	using Widget::bounds;

	void mouseButton(const MouseButtonEvent&) override;
	void focus(bool gained) override;
	void textInput(const TextInputEvent&) override;
	void key(const KeyEvent&) override;

	void update(double delta) override;
	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
	void updateCursorPosition();
	void cursor(bool show, bool resetBlink = true);

protected:
	static constexpr auto padding = Vec {10.f, 10.f};

	struct {
		struct {
			RectShape shape;
			Paint paint;
		} bg;

		struct {
			RectShape shape;
		} cursor;

		struct {
			Text text;
			Paint paint;
		} label;
	} draw_;

	unsigned cursor_ {};
	bool focus_ {false};
	double blinkAccum_ {};
};

class ColorPicker : public Widget {
public:
	Callback<void(ColorPicker&)> onPick;
	vgv::Color picked;

public:
	ColorPicker(Gui&, Vec2f pos, Vec2f size);

	void bounds(const Rect2f& bounds) override;
	using Widget::bounds;

	void mouseButton(const MouseButtonEvent&) override;
	void mouseMove(const MouseMoveEvent&) override;
	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

	void pick(const vgv::Color&);

protected:
	void layout(Vec2f pos, Vec2f size);
	void click(Vec2f pos);
	void pick(Vec2f pos);

protected:
	static constexpr auto hueWidth_ = 20.f;
	static constexpr auto hueSteps_ = 6u;
	static constexpr auto hueMarkerThickness_ = 4.f;
	static constexpr auto hueMarkerHeight_ = 8.f;
	static constexpr auto colorMarkerRadius_ = 3.f;
	static constexpr auto colorMarkerThickness_ = 1.5f;
	static constexpr auto defaultSelected_ = Vec {0.5f, 0.5f};

	Shape hue_;
	RectShape hueMarker_;

	RectShape selector_;
	CircleShape colorMarker_;

	float xEndSel_ {};
	float xBegHue_ {};
	float currentHue_ {};

	Paint basePaint_ {};
	Paint sGrad_ {}; // saturation gradient
	Paint vGrad_ {}; // value gradient

	Paint stroke_;
	Vec2f selected_ = defaultSelected_;

	bool sliding_ {};

	struct {
		bool hue : 1;
		bool hueMarker : 1;
		bool selector : 1;
		bool colorMarker : 1;
		bool basePaint : 1;
		bool grads : 1;
	} ud_ {}; // updateDevice
};

class ContainerWidget : public Widget, public WidgetContainer {
public:
	ContainerWidget(Gui& gui);
	ContainerWidget(Gui& gui, const Rect2f& bounds);

	using Widget::bounds;

	void mouseMove(const MouseMoveEvent&) override;
	void mouseButton(const MouseButtonEvent&) override;
	void mouseWheel(const MouseWheelEvent&) override;
	void key(const KeyEvent&) override;
	void textInput(const TextInputEvent&) override;

	void focus(bool) override;
	void mouseOver(bool) override;

	Widget* focus() const override;
	Widget* mouseOver() const override;

	void draw(const DrawInstance&) const override;
	const auto& gui() const { return Widget::gui; }
};

class Window : public ContainerWidget {
public:
	Window(Gui& gui, Vec2f pos, Vec2f size);

	Widget& add(std::unique_ptr<Widget>) override;
	void bounds(const Rect2f& bounds) override;
	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
	static constexpr auto outerPadding = Vec {20.f, 20.f};
	static constexpr auto innerPadding = 10.f;

	RectShape bg_;
	Paint bgPaint_;
	Paint borderPaint_;
};

class Row : public ContainerWidget {
public:
	Row(Gui& gui, Vec2f pos, float height = autoSize,
		float widgetWidth = autoSize);

	Widget& add(std::unique_ptr<Widget>) override;
	void bounds(const Rect2f& bounds) override;

protected:
	static constexpr auto outerPadding = Vec {0.f, 0.f};
	static constexpr auto innerPadding = 10.f;

	float height_ {autoSize};
	float widgetWidth_ {autoSize};
};

class Slider : public Widget {
public:
	Callback<void(float)> onChange;
	Callback<void(float)> onSet;

public:
	Slider(Gui& gui, Vec2f pos, float width, float current = 0.5f);

	void bounds(const Rect2f& bounds) override;

	void mouseButton(const MouseButtonEvent& ev) override;
	void mouseMove(const MouseMoveEvent& ev) override;

	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

	float current() const { return current_; }
	float current(float set);

protected:
	void updateCurrent(float xpos);
	void updatePositions();

protected:
	static constexpr auto circleRadius = 9.f;
	static constexpr auto padding = Vec {10.f, 20.f + circleRadius};
	static constexpr auto circlePoints = 16u;
	static constexpr auto lineHeight = 4.f;

	float current_ {};
	bool moving_ {};

	RectShape lineLeft_;
	RectShape lineRight_;
	Paint paintLeft_;
	Paint paintRight_;
	CircleShape circle_;
};

} // namespace oui
