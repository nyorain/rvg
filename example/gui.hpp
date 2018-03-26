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

class Gui {
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

	struct Styles {
		ButtonStyle button;
		TextfieldStyle textfield;
	} styles;

public:
	Gui(Context& context, const Font& font, Styles&& s,
		GuiListener& listener = GuiListener::nop());

	void mouseMove(const MouseMoveEvent&);
	bool mouseButton(const MouseButtonEvent&);
	void mouseWheel(const MouseWheelEvent&);
	bool key(const KeyEvent&);
	bool textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	void update(double delta);
	bool updateDevice();
	void draw(const DrawInstance&);

	Widget* mouseOver() const { return mouseOver_; }
	Widget* focus() const { return focus_; }
	Context& context() const { return context_; }

	Widget& add(std::unique_ptr<Widget>);
	const Font& font() const { return font_; }

	template<typename W, typename... Args>
	W& create(Args&&... args) {
		static_assert(std::is_base_of_v<Widget, W>, "Can only create widgets");
		auto widget = std::make_unique<W>(*this, std::forward<Args>(args)...);
		auto& ret = *widget;
		add(std::move(widget));
		return ret;
	}

	/// Registers the widget for the next update/updateDevice calls.
	void addUpdate(Widget&);
	void addUpdateDevice(Widget&);

	GuiListener& listener() { return listener_.get(); }
	void rerecord() { rerecord_ = true; }

protected:
	Context& context_;
	const Font& font_;
	std::reference_wrapper<GuiListener> listener_;
	std::vector<std::unique_ptr<Widget>> widgets_;
	std::unordered_set<Widget*> update_;
	std::unordered_set<Widget*> updateDevice_;
	Widget* mouseOver_ {};
	std::pair<Widget*, MouseButton> buttonGrab_ {};
	Widget* focus_ {};
	bool rerecord_ {};
};

class Widget : public nytl::NonMovable {
public:
	Gui& gui;
	Rect2f bounds {};

public:
	Widget(Gui& xgui) : gui(xgui) {}
	virtual ~Widget() = default;

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

	void registerUpdate();
	void registerUpdateDevice();
};

class Button : public Widget {
public:
	Callback<void(Button&)> onClicked;

public:
	Button(Gui&, Vec2f pos, std::string xlabel);

	void mouseButton(const MouseButtonEvent&) override;
	void mouseOver(bool gained) override;

	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
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
	struct {
		struct {
			RectShape shape;
			Paint paint;
		} bg;

		struct {
			RectShape shape;
			// PaintBinding paint;
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

	void mouseButton(const MouseButtonEvent&) override;
	void mouseMove(const MouseMoveEvent&) override;
	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
	void click(Vec2f pos);
	void pick(Vec2f pos);

protected:
	Shape hue_;
	RectShape selector_;

	float xEndSel_ {};
	float xBegHue_ {};
	float currentHue_ {};

	Paint basePaint_ {};
	Paint sGrad_ {}; // saturation gradient
	Paint vGrad_ {}; // value gradient

	Paint stroke_;
	Vec2f selected_ {1.f, 0.f};

	bool sliding_ {};
};

} // namespace oui
