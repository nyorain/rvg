#pragma once

#include <vgv/vgv.hpp>
#include <nytl/vec.hpp>
#include <nytl/rect.hpp>
#include <nytl/callback.hpp>
#include <nytl/stringParam.hpp>

namespace vui {

using namespace nytl;
using namespace vgv;

// TODO!
/*
enum class MouseButton {
	left,
	right,
	middle,
	custom1,
	custom2
};

enum class Key {
	enter,
	left,
	right,
	escape
};
*/

// events
struct MouseMoveEvent {
	Vec2f position;
};

struct MouseButtonEvent {
	bool pressed;
	unsigned int button;
	Vec2f position;
};

struct MouseWheelEvent {
	float distance;
};

struct KeyEvent {
	unsigned int key;
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
		std::reference_wrapper<const PaintBuffer> label;
		std::reference_wrapper<const PaintBuffer> bg;
	};

	struct ButtonStyle {
		ButtonDraw normal;
		ButtonDraw hovered;
		ButtonDraw pressed;
	};

	struct TextfieldStyle {
		std::reference_wrapper<const PaintBuffer> label;
		std::reference_wrapper<const PaintBuffer> bg;
	};

	struct Styles {
		ButtonStyle button;
		TextfieldStyle textfield;
	} styles;

public:
	Gui(Context& context, const Font& font, Styles&& s,
		GuiListener& listener = GuiListener::nop());

	void mouseMove(const MouseMoveEvent&);
	void mouseButton(const MouseButtonEvent&);
	void mouseWheel(const MouseWheelEvent&);
	void key(const KeyEvent&);
	void textInput(const TextInputEvent&);
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
	std::vector<Widget*> update_;
	std::vector<Widget*> updateDevice_;
	Widget* mouseOver_ {};
	std::pair<Widget*, unsigned> buttonGrab_ {};
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
			PaintBinding paint;
		} bg;

		struct {
			Text text;
			PaintBinding paint;
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

	void draw(const DrawInstance&) const override;
	bool updateDevice() override;

protected:
	void updateCursor();

protected:
	struct {
		struct {
			RectShape shape;
			PaintBinding paint;
		} bg;

		struct {
			RectShape shape;
			// PaintBinding paint;
		} cursor;

		struct {
			Text text;
			PaintBinding paint;
		} label;
	} draw_;

	unsigned cursor_;
	bool focus_;
};

} // namespace oui
