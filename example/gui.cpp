#include "gui.hpp"
#include <dlg/dlg.hpp>
#include <nytl/rectOps.hpp>
#include <nytl/utf.hpp>
#include <cmath>

namespace vui {
namespace {

template<typename T>
Vec2<T> rb(const Rect2<T>& r) { // right bottom
	return r.position + r.size;
}

template<typename T>
Vec2<T> rt(const Rect2<T>& r) { // right bottom
	return r.position + Vec {r.size.x, 0.f};
}

} // anon namespace

// GuiListener
GuiListener& GuiListener::nop() {
	static GuiListener listener;
	return listener;
}

// WidgetContainer
void WidgetContainer::mouseMove(const MouseMoveEvent& ev) {
	for(auto& w : widgets_) {
		if(contains(w->bounds(), ev.position)) {
			if(w.get() != mouseOver_) {
				if(mouseOver_) {
					mouseOver_->mouseOver(false);
				}

				mouseOver_ = w.get();
				w->mouseOver(true);
			}

			w->mouseOver(true);
			w->mouseMove(ev);
			return;
		}
	}

	if(mouseOver_) {
		mouseOver_->mouseOver(false);
		mouseOver_ = nullptr;
	}
}

bool WidgetContainer::mouseButton(const MouseButtonEvent& ev) {
	if(mouseOver_ != focus_) {
		if(focus_) {
			focus_->focus(false);
		}

		focus_ = mouseOver_;
		if(focus_) {
			focus_->focus(true);
		}
	}

	if(mouseOver_) {
		mouseOver_->mouseButton(ev);
	}

	return mouseOver_;
}
void WidgetContainer::mouseWheel(const MouseWheelEvent& ev) {
	if(mouseOver_) {
		mouseOver_->mouseWheel(ev);
	}
}
bool WidgetContainer::key(const KeyEvent& ev) {
	if(focus_) {
		focus_->key(ev);
	}

	return focus_;
}
bool WidgetContainer::textInput(const TextInputEvent& ev) {
	if(focus_) {
		focus_->textInput(ev);
	}

	return focus_;
}
void WidgetContainer::focus(bool gained) {
	if(!gained && focus_) {
		focus_->focus(false);
		focus_ = nullptr;
	}
}
void WidgetContainer::mouseOver(bool gained) {
	if(!gained && mouseOver_) {
		mouseOver_->mouseOver(false);
		mouseOver_ = nullptr;
	}
}

Widget* WidgetContainer::mouseOver() const {
	if(!mouseOver_) {
		return nullptr;
	}

	auto traversed = mouseOver_->mouseOver();
	return traversed ? traversed : mouseOver_;
}

Widget* WidgetContainer::focus() const {
	if(!focus_) {
		return nullptr;
	}

	auto traversed = focus_->focus();
	return traversed ? traversed : focus_;
}

void WidgetContainer::draw(const DrawInstance& di) const {
	for(auto& widget : widgets_) {
		dlg_assert(widget);
		widget->draw(di);
	}
}

Widget& WidgetContainer::add(std::unique_ptr<Widget> widget) {
	dlg_assert(widget);
	widgets_.push_back(std::move(widget));
	return *widgets_.back();
}

// Gui
Gui::Gui(Context& ctx, const Font& font, Styles&& s, GuiListener& listener)
	: WidgetContainer(*this), styles(std::move(s)), context_(ctx),
		font_(font), listener_(listener) {
}

void Gui::mouseMove(const MouseMoveEvent& ev) {
	auto o = WidgetContainer::mouseOver();
	WidgetContainer::mouseMove(ev);
	auto n = WidgetContainer::mouseOver();
	if(o != n) {
		listener().mouseOver(o, n);
	}
}

bool Gui::mouseButton(const MouseButtonEvent& ev) {
	if(!ev.pressed && buttonGrab_.first && ev.button == buttonGrab_.second) {
		buttonGrab_.first->mouseButton(ev);
		buttonGrab_ = {};
		return true;
	} else {
		auto o = WidgetContainer::focus();
		WidgetContainer::mouseButton(ev);
		auto n = WidgetContainer::focus();
		if(o != n) {
			listener().focus(o, n);
		}

		if(ev.pressed && n) {
			// TODO: send release to old buttonGrab_?
			//   or use multiple button grabs?
			buttonGrab_ = {n, ev.button};
		}

		return n;
	}
}

void Gui::focus(bool gained) {
	auto o = WidgetContainer::focus();
	WidgetContainer::focus(gained);
	auto n = WidgetContainer::focus();
	if(o != n) {
		listener().focus(o, n);
	}
}

void Gui::mouseOver(bool gained) {
	auto o = WidgetContainer::mouseOver();
	WidgetContainer::mouseOver(gained);
	auto n = WidgetContainer::mouseOver();
	if(o != n) {
		listener().mouseOver(o, n);
	}
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
	bool rerecord = rerecord_;

	for(auto& widget : moved) {
		dlg_assert(widget);
		rerecord |= widget->updateDevice();
	}

	rerecord_ = false;
	return rerecord;
}

void Gui::addUpdate(Widget& widget) {
	update_.insert(&widget);
}
void Gui::addUpdateDevice(Widget& widget) {
	updateDevice_.insert(&widget);
}

// Widget
bool Widget::contains(Vec2f point) const {
	return nytl::contains(bounds(), point);
}

void Widget::bounds(const Rect2f& newBounds) {
	bounds_ = newBounds;
}

void Widget::position(Vec2f pos) {
	bounds({pos, size()});
}

void Widget::size(Vec2f size) {
	bounds({position(), size});
}

void Widget::registerUpdate() {
	gui.addUpdate(*this);
}

void Widget::registerUpdateDevice() {
	gui.addUpdateDevice(*this);
}

// ContainerWidget
ContainerWidget::ContainerWidget(Gui& gui)
	: Widget(gui), WidgetContainer(gui) {
}

ContainerWidget::ContainerWidget(Gui& gui, const Rect2f& bounds)
	: Widget(gui, bounds), WidgetContainer(gui) {
}

void ContainerWidget::mouseMove(const MouseMoveEvent& ev) {
	WidgetContainer::mouseMove(ev);
}

void ContainerWidget::mouseButton(const MouseButtonEvent& ev) {
	WidgetContainer::mouseButton(ev);
}

void ContainerWidget::mouseWheel(const MouseWheelEvent& ev) {
	WidgetContainer::mouseWheel(ev);
}

void ContainerWidget::key(const KeyEvent& ev) {
	WidgetContainer::key(ev);
}

void ContainerWidget::textInput(const TextInputEvent& ev) {
	WidgetContainer::textInput(ev);
}

void ContainerWidget::focus(bool gained) {
	return WidgetContainer::focus(gained);
}

void ContainerWidget::mouseOver(bool gained) {
	return WidgetContainer::mouseOver(gained);
}

void ContainerWidget::draw(const DrawInstance& di) const {
	WidgetContainer::draw(di);
}

Widget* ContainerWidget::focus() const {
	return WidgetContainer::focus();
}

Widget* ContainerWidget::mouseOver() const {
	return WidgetContainer::mouseOver();
}

// Button
Button::Button(Gui& gui, Vec2f pos, std::string label) : Widget(gui) {

	auto& ctx = gui.context();
	auto& font = gui.font();
	draw_.label.text = {ctx, label, font, pos + padding};
	draw_.label.paint = {ctx, gui.styles.button.normal.label};
	auto size = 2 * padding + Vec {font.width(label), font.height()};
	draw_.bg.shape = {ctx, pos, size, DrawMode {true, 2.f}};
	draw_.bg.paint = {ctx, gui.styles.button.normal.bg};

	bounds_ = {pos, size};
	draw_.label.text.updateDevice(ctx);
	draw_.bg.shape.updateDevice(ctx);
}

void Button::bounds(const Rect2f& bounds) {
	Widget::bounds(bounds);

	draw_.label.text.position = position() + padding;
	draw_.label.text.update();
	draw_.bg.shape.position = position();
	draw_.bg.shape.update();
	registerUpdateDevice();
}

void Button::mouseButton(const MouseButtonEvent& event) {
	if(event.button != MouseButton::left) {
		return;
	}

	if(event.pressed) {
		pressed_ = true;
	} else if(pressed_) {
		pressed_ = false;
		if(hovered_) {
			onClicked(*this);
		}
	}

	registerUpdateDevice();
}
void Button::mouseOver(bool gained) {
	if(gained != hovered_) {
		registerUpdateDevice();
	}
	hovered_ = gained;
}

void Button::draw(const DrawInstance& di) const {
	draw_.bg.paint.bind(di);
	draw_.bg.shape.fill(di);

	draw_.label.paint.bind(di);
	draw_.label.text.draw(di);

	draw_.bg.shape.stroke(di);
}

bool Button::updateDevice() {
	auto re = false;
	auto& ctx = gui.context();
	auto& bs = gui.styles.button;
	auto& draw = pressed_ ? bs.pressed : hovered_ ? bs.hovered : bs.normal;

	draw_.bg.paint.paint = draw.bg;
	re |= draw_.bg.paint.updateDevice(ctx);

	draw_.label.paint.paint = draw.label;
	re |= draw_.label.paint.updateDevice(ctx);

	// TODO(performance): make optional
	re |= draw_.bg.shape.updateDevice(ctx);
	re |= draw_.label.text.updateDevice(ctx);

	return re;
}

// Textfield
Textfield::Textfield(Gui& gui, Vec2f pos, float width) : Widget(gui) {
	auto& ctx = gui.context();

	auto& font = gui.font();
	auto height = font.height() + 2 * padding.y;
	draw_.bg.shape = {ctx, pos, {width, height}, {true, 2.f}};
	draw_.bg.paint = {ctx, gui.styles.textfield.bg};

	auto cursorSize = Vec {1.f, font.height()};
	auto cursorPos = pos + Vec {padding.x, padding.y};
	draw_.cursor.shape = {ctx, cursorPos, cursorSize, {true, 0.f}, true};

	draw_.label.text = {ctx, "", font, pos + padding};
	draw_.label.paint = {ctx, gui.styles.textfield.label};

	bounds_ = {pos, {width, height}};
}

void Textfield::bounds(const Rect2f& bounds) {
	Widget::bounds(bounds);

	draw_.bg.shape.position = position();
	draw_.bg.shape.update();
	draw_.label.text.position = position() + padding;
	draw_.label.text.update();
	draw_.cursor.shape.position.y = position().y + padding.y;
	updateCursorPosition(); // calls registerUpdateDevice
}

void Textfield::mouseButton(const MouseButtonEvent& ev) {
	auto& text = draw_.label.text;
	auto ca = text.charAt(ev.position.x - text.position.x);
	cursor_ = ca.last;
	draw_.cursor.shape.position.x = text.position.x + ca.nearestBoundary;
	draw_.cursor.shape.update();
	registerUpdateDevice();
	dlg_assert(cursor_ <= text.text.length());
}

void Textfield::focus(bool gained) {
	focus_ = gained;
	cursor(focus_, true);
	registerUpdate();
}
void Textfield::textInput(const TextInputEvent& ev) {
	auto utf32 = toUtf32(ev.utf8);
	auto& str = draw_.label.text.text;
	dlg_assert(cursor_ <= str.length());
	str.insert(cursor_, utf32);
	draw_.label.text.update();
	cursor_ += utf32.length();
	updateCursorPosition();
	dlg_assert(cursor_ <= str.length());
	cursor(true, true);
}

void Textfield::key(const KeyEvent& ev) {
	auto& str = draw_.label.text.text;
	dlg_assert(cursor_ <= str.length());

	if(ev.pressed) {
		if(ev.key == Key::backspace && cursor_ > 0) {
			cursor_ -= 1;
			str.erase(cursor_, 1);
			draw_.label.text.update();
			updateCursorPosition();
		} else if(ev.key == Key::left && cursor_ > 0) {
			cursor_ -= 1;
			updateCursorPosition();
			cursor(true, true);
		} else if(ev.key == Key::right && cursor_ < str.length()) {
			cursor_ += 1;
			updateCursorPosition();
			cursor(true, true);
		} else if(ev.key == Key::del && cursor_ < str.length()) {
			str.erase(cursor_, 1);
			draw_.label.text.update();
			registerUpdateDevice();
		}
	}

	dlg_assert(cursor_ <= str.length());
}

void Textfield::draw(const DrawInstance& di) const {
	draw_.bg.paint.bind(di);
	draw_.bg.shape.fill(di);

	draw_.label.paint.bind(di);
	draw_.label.text.draw(di);

	draw_.cursor.shape.fill(di);

	draw_.bg.shape.stroke(di);
}

bool Textfield::updateDevice() {
	auto& ctx = gui.context();
	auto re = false;

	// TODO(performance): only update what is needed (see color picker, flags)
	re |= draw_.label.text.updateDevice(ctx);
	re |= draw_.bg.shape.updateDevice(ctx);
	re |= draw_.cursor.shape.updateDevice(ctx);

	return re;
}

void Textfield::update(double delta) {
	constexpr auto blinkTime = 0.5;
	blinkAccum_ = blinkAccum_ + delta;
	if(blinkAccum_ > blinkTime) {
		blinkAccum_ = std::fmod(blinkAccum_, blinkTime);
		draw_.cursor.shape.hide ^= true;
		registerUpdateDevice();
	}

	if(focus_) {
		registerUpdate();
	}
}

void Textfield::updateCursorPosition() {
	auto len = draw_.label.text.text.size();
	dlg_assert(cursor_ <= len);
	auto x = position().x + padding.x;
	if(cursor_ > 0) {
		if(cursor_ == len) {
			auto b = draw_.label.text.ithBounds(cursor_ - 1);
			x += b.position.x + b.size.x;
		} else {
			x += draw_.label.text.ithBounds(cursor_).position.x;
		}
	}

	draw_.cursor.shape.position.x = x;
	draw_.cursor.shape.update();

	registerUpdateDevice();
}

void Textfield::cursor(bool show, bool resetBlink) {
	draw_.cursor.shape.hide = !show;
	registerUpdateDevice();
	if(resetBlink) {
		blinkAccum_ = 0.f;
	}
}

// ColorPicker
ColorPicker::ColorPicker(Gui& gui, Vec2f pos, Vec2f size) :
		Widget(gui, {pos, size}) {
	auto& ctx = gui.context();

	stroke_ = {ctx, vgv::colorPaint({0, 0, 0})};
	selector_.draw = {true, 2.f};

	auto drawMode = DrawMode {false, hueWidth_};
	drawMode.color.fill = false;
	drawMode.color.stroke = true;

	for(auto i = 0u; i < hueSteps_ + 1; ++i) {
		auto col = hsvNorm(i / float(hueSteps_), 1.f, 1.f);
		drawMode.color.points.push_back(col.rgba());
	}

	hue_.draw = drawMode;

	picked = hsv(currentHue_, 255.f * selected_.x, 255.f * selected_.y);
	basePaint_ = {ctx, vgv::colorPaint(picked)};

	hueMarker_.draw = {false, hueMarkerThickness_};
	colorMarker_.draw = {false, colorMarkerThickness_};
	colorMarker_.points = 6u;

	layout(pos, size);
	updateDevice();
}

void ColorPicker::layout(Vec2f pos, Vec2f size) {
	constexpr auto hueWidth = 20.f;
	constexpr auto huePad = 10.f;

	xBegHue_ = pos.x + size.x - hueWidth;
	xEndSel_ = xBegHue_ - huePad;

	// selector
	selector_.position = pos;
	selector_.size = size - Vec {hueWidth + huePad, 0};
	selector_.update();
	ud_.selector = true;

	// hue
	hue_.points.clear();

	auto ystep = size.y / float(hueSteps_);
	auto x = pos.x + size.x - hueWidth / 2;
	for(auto i = 0u; i < hueSteps_ + 1; ++i) {
		hue_.points.push_back({x, pos.y + ystep * i});
	}

	hue_.update();
	ud_.hue = true;

	// selector grads
	sGrad_.paint = vgv::linearGradient(pos, Vec {xEndSel_, pos.y},
		hsv(0, 0, 255), hsv(0, 0, 255, 0));
	vGrad_.paint = vgv::linearGradient(pos, pos + Vec {0, size.y},
		hsv(0, 255, 0, 0), hsv(0, 255, 0));
	ud_.grads = true;

	// hue marker
	hueMarker_.position = {xBegHue_, pos.y - hueMarkerThickness_ / 2},
	hueMarker_.size = {hueWidth, hueMarkerHeight_};
	hueMarker_.update();
	ud_.hueMarker = true;

	// color marker
	using namespace nytl::vec::cw::operators;
	auto cpos = selector_.position + selected_ * selector_.size;
	colorMarker_.center = cpos;
	colorMarker_.radius = {colorMarkerRadius_, colorMarkerRadius_};
	colorMarker_.update();
	ud_.colorMarker = true;
}

void ColorPicker::bounds(const Rect2f& bounds) {
	Widget::bounds(bounds);
	layout(position(), size());
	registerUpdateDevice();
}

void ColorPicker::mouseButton(const MouseButtonEvent& ev) {
	if(ev.button != MouseButton::left) {
		return;
	}

	sliding_ = ev.pressed;
	if(ev.pressed) {
		click(ev.position);
	}
}

void ColorPicker::mouseMove(const MouseMoveEvent& ev) {
	if(sliding_) {
		click(ev.position);
	}
}

void ColorPicker::draw(const DrawInstance& di) const {
	auto& ctx = gui.context();

	for(auto* p : {&basePaint_, &sGrad_, &vGrad_}) {
		p->bind(di);
		selector_.fill(di);
	}

	ctx.pointColorPaint().bind(di);
	hue_.stroke(di);

	stroke_.bind(di);
	selector_.stroke(di);
	hueMarker_.stroke(di);
	colorMarker_.stroke(di);
}

bool ColorPicker::updateDevice() {
	auto& ctx = gui.context();
	auto ret = false;
	ret |= (ud_.hue && hue_.updateDevice(ctx));
	ret |= (ud_.hueMarker && hueMarker_.updateDevice(ctx));
	ret |= (ud_.selector && selector_.updateDevice(ctx));
	ret |= (ud_.colorMarker && colorMarker_.updateDevice(ctx));
	ret |= (ud_.basePaint && basePaint_.updateDevice(ctx));
	ret |= (ud_.grads && (sGrad_.updateDevice(ctx) | vGrad_.updateDevice(ctx)));
	ud_ = {};
	return ret;
}

void ColorPicker::pick(Vec2f f) {
	picked = hsvNorm(currentHue_, f.x, 1 - f.y);
	onPick(*this);
}

void ColorPicker::click(Vec2f pos) {
	if(pos.x < xEndSel_) {
		using namespace nytl::vec::cw::operators;
		selected_ = (pos - selector_.position) / selector_.size;
		pick(selected_);

		colorMarker_.center = pos;
		colorMarker_.update();
		ud_.colorMarker = true;
		registerUpdateDevice();
	} else if(pos.x > xBegHue_) {
		auto y = (pos.y - hue_.points[0].y) / selector_.size.y;
		currentHue_ = std::clamp(y, 0.f, 1.f);
		pick(selected_);

		basePaint_.paint.data.frag.inner = hsvNorm(currentHue_, 1.f, 1.f);
		ud_.basePaint = true;

		hueMarker_.position.y = pos.y - hueMarker_.size.y / 2.f;
		hueMarker_.update();
		ud_.hueMarker = true;
		registerUpdateDevice();
	}
}

// Window
Window::Window(Gui& gui, Vec2f pos, Vec2f size) :
		ContainerWidget(gui, {pos, size}) {

	bg_ = {gui.context(), pos, size, {true, 2.f}};
	bgPaint_ = {gui.context(), gui.styles.window.bg};
	borderPaint_ = {gui.context(), gui.styles.window.border};
}

void Window::bounds(const Rect2f& bounds) {
	if(bounds.position != position()) {
		auto pos = bounds.position + outerPadding;
		for(auto& w : widgets_) {
			w->position(pos);
			pos.y += w->size().y + innerPadding;
		}
	}

	Widget::bounds(bounds);

	bg_.position = bounds.position;
	bg_.size = bounds.size;
	bg_.update();
	registerUpdateDevice();
}

Widget& Window::add(std::unique_ptr<Widget> w) {
	auto pos = position() + outerPadding;
	if(!widgets_.empty()) {
		auto b = widgets_.back()->bounds();
		pos.y = b.position.y + b.size.y + innerPadding;
	}

	w->position(pos);
	return ContainerWidget::add(std::move(w));
}

bool Window::updateDevice() {
	auto& ctx = gui().context();
	return bg_.updateDevice(ctx);
}

void Window::draw(const DrawInstance& di) const {
	bgPaint_.bind(di);
	bg_.fill(di);

	borderPaint_.bind(di);
	bg_.stroke(di);

	ContainerWidget::draw(di);
}

// Row
Row::Row(Gui& gui, Vec2f pos, float height, float widgetWidth)
	: ContainerWidget(gui, {pos, {autoSize, height}}), height_(height),
		widgetWidth_(widgetWidth) {
}

void Row::bounds(const Rect2f& bounds) {
	if(bounds.position != position()) {
		auto pos = bounds.position + outerPadding;
		for(auto& w : widgets_) {
			w->position(pos);
			pos.x += w->size().x + innerPadding;
		}
	}

	Widget::bounds(bounds);
}

Widget& Row::add(std::unique_ptr<Widget> w) {
	auto pos = position() + outerPadding;
	if(!widgets_.empty()) {
		pos = rt(widgets_.back()->bounds());
		pos.x += innerPadding;
	}

	auto size = w->size();
	if(height_ != autoSize) {
		size.y = height_;
	}

	if(widgetWidth_ != autoSize) {
		size.x = widgetWidth_;
	}

	w->bounds({pos, size});
	bounds_.size.y = std::max(bounds_.size.y, w->size().y);
	bounds_.size.x = rt(w->bounds()).x - position().x;

	return ContainerWidget::add(std::move(w));
}

} // namespace vui
