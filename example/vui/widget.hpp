#pragma once

#include "fwd.hpp"
#include "input.hpp"

#include <rvg/state.hpp>

#include <nytl/nonCopyable.hpp>
#include <nytl/vec.hpp>
#include <nytl/rect.hpp>

namespace vui {

/// Can be passed to a Widget for size to let it choose its own size.
constexpr auto autoSize = -1.f;

/// Interface for an control on the screen that potentially handles input
/// events and can draw itself. Typical implementations of this interface
/// are Button, Textfield or ColorPicker.
class Widget : public nytl::NonMovable {
public:
	Widget(Gui&, const Rect2f& bounds = {});
	virtual ~Widget() = default;

	/// Hides/unhides this widget.
	/// A hidden widget should not render anything.
	virtual void hide(bool hide) = 0;

	/// Returns whether the widget is hidden.
	virtual bool hidden() const = 0;

	/// Resizes this widget. Note that not all widgets are resizable equally.
	/// Some might throw when an invalid size is given or just display
	/// their content incorrectly.
	virtual void size(Vec2f) = 0;

	/// Changes the widgets global position.
	virtual void position(Vec2f);

	/// Refreshes the premultiplied transform from the gui object.
	virtual void refreshTransform();

	/// Returns whether the widget contains the given point in global
	/// coordinates.
	/// Does not take any premultiplied transform into account.
	virtual bool contains(Vec2f point) const;

	/// Instructs the widget to intersect its own scissor with the given rect.
	/// Resets any previous scissor intersections. Given in global coordinates.
	virtual void intersectScissor(const Rect2f&);

	/// Called when the Widget has registered itself for update.
	/// Gets the delta time since the last frame in seconds.
	/// Must not touch resources used for rendering.
	virtual void update(double) {}

	/// Called when the Widget has registered itself for updateDevice.
	/// Called when no rendering is currently done, so the widget might
	/// update rendering resources.
	virtual bool updateDevice() { return false; }

	/// Called during a drawing instance.
	/// The widget should draw itself.
	virtual void draw(const DrawInstance&) const {}

	/// The z order of this widget.
	/// Widgets	with a lower z order are drawn first.
	/// Note that the zOrder only matters for widgets with the same parent.
	virtual int zOrder() const { return 0; }

	// - input processing -
	// all positions are given in global window coordinates
	// return the Widget that processed the event which might be itself
	// or a child widget or none (nullptr).
	virtual Widget* mouseMove(const MouseMoveEvent&) { return nullptr; }
	virtual Widget* mouseButton(const MouseButtonEvent&) { return nullptr; }
	virtual Widget* mouseWheel(const MouseWheelEvent&) { return nullptr; }
	virtual Widget* key(const KeyEvent&) { return nullptr; }
	virtual Widget* textInput(const TextInputEvent&) { return nullptr; }

	virtual void focus(bool) {}
	virtual void mouseOver(bool) {}

	/// Returns the associated gui object.
	Gui& gui() const { return gui_; }
	Context& context() const;

	const auto& bounds() const { return bounds_; }
	const auto& position() const { return bounds_.position; }
	const auto& size() const { return bounds_.size; }

protected:
	/// Registers this widgets for an update/updateDevice callback as soon
	/// as possible.
	void registerUpdate();
	void registerUpdateDevice();

	/// Must be called when bounds_ was changed
	void updateScissor();

	/// Utility to bind scissor and transform to the given DrawInstance.
	void bindState(const DrawInstance&) const;

	/// Returns its own scissor rect in local coordinates.
	virtual Rect2f ownScissor() const;

protected:
	Gui& gui_; // associated gui
	Rect2f bounds_; // global bounds
	Transform transform_;
	Scissor scissor_;
	Rect2f intersectScissor_;
};

} // namespace vui
