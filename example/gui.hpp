#pragma once

#include <vgv/vgv.hpp>
#include <nytl/vec.hpp>
#include <nytl/rect.hpp>
#include <nytl/callback.hpp>
#include <nytl/stringParam.hpp>

namespace vui {

using namespace nytl;
using namespace vgv;

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
		Paint buttonLabel;
		Paint buttonBackground;
	};

	struct ButtonStyle {
		ButtonDraw normal;
		ButtonDraw hovered;
		ButtonDraw pressed;
	};

	struct {
		ButtonStyle button;

		Paint textfieldBackground;
		Paint textfieldText;
	} styles;

public:
	Gui(Context& context, GuiListener& listener = GuiListener::nop());

	void mouseMove(const MouseMoveEvent&);
	void mouseButton(const MouseButtonEvent&);
	void mouseWheel(const MouseWheelEvent&);
	void key(const KeyEvent&);
	void textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	void update(double delta);
	bool updateDevice();
	void draw(vk::CommandBuffer);

	Widget* mouseOver() const { return mouseOver_; }
	Widget* focus() const { return focus_; }
	Context& context() const { return context_; }

	Widget& add(std::unique_ptr<Widget>);

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

protected:
	Context& context_;
	std::reference_wrapper<GuiListener> listener_;
	std::vector<std::unique_ptr<Widget>> widgets_;
	std::vector<Widget*> update_;
	std::vector<Widget*> updateDevice_;
	Widget* mouseOver_ {};
	Widget* focus_ {};
};

class Widget : public nytl::NonMovable {
public:
	Gui& gui;
	Rect2f bounds {};

public:
	Widget(Gui& xgui) : gui(xgui) {}

	virtual void mouseMove(const MouseMoveEvent&) {}
	virtual void mouseButton(const MouseButtonEvent&) {}
	virtual void mouseWheel(const MouseWheelEvent&) {}
	virtual void key(const KeyEvent&) {}
	virtual void textInput(const TextInputEvent&) {}

	virtual void focus(bool) {}
	virtual void mouseOver(bool) {}

	virtual void draw(vk::CommandBuffer) {}
	virtual void update(double) {}
	virtual bool updateDevice() { return false; }

	void registerUpdate();
	void registerUpdateDevice();
};

class Button : public Widget {
public:
	Callback<void(Button&)> onClicked;
	std::string label;

public:
	Button(Gui&, std::string xlabel = {});

	void mouseButton(const MouseButtonEvent&) override;
	void mouseOver(bool gained) override;

	void draw(vk::CommandBuffer) override;
	bool updateDevice() override;

protected:
	struct {
		Polygon background;
		Text label;
	} draw_;

	bool mouseOver_ {};
	bool pressed_ {};
};

} // namespace oui
