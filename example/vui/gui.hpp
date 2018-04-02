#pragma once

#include "fwd.hpp"
#include "input.hpp"
#include "style.hpp"
#include "container.hpp"

#include <vgv/vgv.hpp>

#include <nytl/nonCopyable.hpp>
#include <nytl/vec.hpp>
#include <nytl/rect.hpp>
#include <nytl/stringParam.hpp>

#include <unordered_set>
#include <optional>

namespace vui {

/// Can be implemented and passed to a Gui object to get notified about
/// certain events.
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

/// Central gui object. Manages all widgets and is entry point for
/// input and rendering.
class Gui : public WidgetContainer, public nytl::NonCopyable {
public:
	/// Constants to be parameterized
	static constexpr auto hintDelay = 1.f; // seconds
	static constexpr auto hintOffset = Vec {20.f, 5.f}; // seconds

public:
	Gui(Context& context, const Font& font, Styles&& s,
		GuiListener& listener = GuiListener::nop());

	/// Makes the Gui process the given input.
	Widget* mouseMove(const MouseMoveEvent&);
	Widget* mouseButton(const MouseButtonEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	/// Update should be called every frame (or otherwise as often as
	/// possible) with the delta frame time in seconds.
	/// Needed for time-sensitive stuff like animations or cusor blinkin.
	void update(double delta);

	/// Should be called once every frame when the device is not rendering.
	/// Will update device resources.
	bool updateDevice();

	/// Changes the transform to use for all widgets.
	void transform(const nytl::Mat4f&);

	/// Returns a descendent (so e.g. the child of a child) which currently
	/// has focus/over which the mouse hovers.
	/// Effectively traverses the line of focused/mouseOver children.
	/// Returns nullptr if there is currently none.
	Widget* mouseOver() const { return mouseOverWidget_; }
	Widget* focus() const { return focusWidget_; }

	/// Adds an already created widget to this gui object.
	using WidgetContainer::add;
	using WidgetContainer::create;

	Context& context() const { return context_; }
	const Font& font() const { return font_; }
	const nytl::Mat4f transform() const { return transform_; }
	const auto& styles() const { return styles_; }

	GuiListener& listener() { return listener_.get(); }
	void rerecord() { rerecord_ = true; }

	/// Registers the widget for the next update/updateDevice calls.
	void addUpdate(Widget&);
	void addUpdateDevice(Widget&);

protected:
	Context& context_;
	const Font& font_;
	std::reference_wrapper<GuiListener> listener_;
	std::unordered_set<Widget*> update_;
	std::unordered_set<Widget*> updateDevice_;
	std::pair<Widget*, MouseButton> buttonGrab_ {};
	bool rerecord_ {};
	nytl::Mat4f transform_ {};
	Styles styles_;

	Widget* focusWidget_ {};
	Widget* mouseOverWidget_ {};
};

} // namespace vui
