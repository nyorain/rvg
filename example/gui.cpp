#include "gui.hpp"
#include <dlg/dlg.hpp>

namespace vui {

// GuiListener
GuiListener& GuiListener::nop() {
	static GuiListener listener;
	return listener;
}

// Gui
Gui::Gui(Context& context, GuiListener& listener) : context_(context),
		listener_(listener) {
}

void Gui::mouseMove(const MouseMoveEvent&) {
}
void Gui::mouseButton(const MouseButtonEvent&) {
}
void Gui::key(const KeyEvent&) {
}
void Gui::textInput(const TextInputEvent&) {
}
void Gui::focus(bool gained) {
}
void Gui::mouseOver(bool gained) {
}

// NOTE: can be optimized (moved vectors could be cached for less
// allocations).
void Gui::update(double delta) {
	auto moved = std::move(update_);
	for(auto& widget : moved) {
		dlg_assert(widget);
		widget->update(delta);
	}
}
bool Gui::updateDevice() {
	auto moved = std::move(updateDevice_);
	bool rerecord = false;
	for(auto& widget : moved) {
		dlg_assert(widget);
		rerecord |= widget->updateDevice();
	}

	return rerecord;
}
void Gui::draw(vk::CommandBuffer cmdb) {
	for(auto& widget : widgets_) {
		dlg_assert(widget);
		widget->draw(cmdb);
	}
}

Widget& Gui::add(std::unique_ptr<Widget> widget) {
	dlg_assert(widget);
	widgets_.push_back(std::move(widget));
	return *widgets_.back();
}

void Gui::addUpdate(Widget& widget) {
	update_.push_back(&widget);
}
void Gui::addUpdateDevice(Widget& widget) {
	updateDevice_.push_back(&widget);
}

// Widget
void Widget::registerUpdate() {
	gui.addUpdate(*this);
}
void Widget::registerUpdateDevice() {
	gui.addUpdateDevice(*this);
}

// Button
Button::Button(Gui& gui, std::string xlabel) :
		Widget(gui), label(std::move(xlabel)) {
}

void Button::mouseButton(const MouseButtonEvent& event) {
}
void Button::mouseOver(bool gained) {
	if(gained != mouseOver_) {
		registerUpdateDevice();
	}
	mouseOver_ = gained;
}

void Button::draw(vk::CommandBuffer cmdb) {
	auto& ctx = gui.context();
	gui.paints.buttonBackground.bind(ctx, cmdb);
	draw_.background.fill(ctx, cmdb);

	gui.paints.buttonLabel.bind(ctx, cmdb);
	draw_.label.draw(ctx, cmdb);
}
bool Button::updateDevice() {
	auto& ctx = gui.context();
	return draw_.background.updateDevice(ctx, DrawMode::fill) ||
		draw_.label.updateDevice(ctx);
}

} // namespace vui
