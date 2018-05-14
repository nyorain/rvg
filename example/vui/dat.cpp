#include "dat.hpp"
#include "gui.hpp"
#include <rvg/font.hpp>

#include <dlg/dlg.hpp>

namespace vui::dat {
namespace colors {

const auto name = rvg::Color {255u, 255u, 255u};
const auto line = rvg::Color {20u, 20u, 20u, 200u};
const auto button = rvg::Color {200u, 20u, 20u};
const auto text = rvg::Color {20u, 120u, 20u};
const auto label = rvg::Color {20u, 120u, 120u};
const auto range = rvg::Color {20u, 20u, 120u};
const auto checkbox = rvg::Color {120u, 20u, 120u};
const auto bg = rvg::Color {10u, 10u, 10u};
const auto bgHover = rvg::Color {5u, 5u, 5u};
const auto bgActive = rvg::Color {2u, 2u, 2u};
const auto bgWidget = rvg::Color {30u, 30u, 30u};

} // namespace colors

constexpr auto classifierWidth = 2.5f;

// Controller
Controller::Controller(Panel& panel, Vec2f pos,
	std::string_view name) :
		Widget(panel.gui(), {pos,
			{panel.position().x + panel.width() - pos.x, panel.rowHeight()}}),
		panel_(panel) {

	auto& ctx = panel.context();
	auto height = panel.rowHeight();

	auto toff = (height - panel.gui().font().height()) / 2;
	auto tpos = Vec {classifierWidth + std::max(toff, classifierWidth), toff};
	name_ = {ctx, name, panel.gui().font(), tpos};

	auto start = Vec{classifierWidth / 2.f, 0.f};
	auto end = Vec{start.x, panel.rowHeight()};
	classifier_ = {ctx, {start, end}, {false, 3.f}};

	start = Vec{0.f, size().y};
	end = Vec{size().x, size().y};
	bottomLine_ = {ctx, {start, end}, {false, 1.f}};

	bg_ = {ctx, {}, size(), {true, 0.f}};
}

void Controller::draw(const rvg::DrawInstance& di) const {
	Widget::bindState(di);

	bgPaint().bind(di);
	bg_.fill(di);

	classPaint().bind(di);
	classifier_.stroke(di);

	panel_.paints().name.bind(di);
	name_.draw(di);

	panel_.paints().line.bind(di);
	bottomLine_.stroke(di);
}

const rvg::Paint& Controller::bgPaint() const {
	return panel().paints().bg;
}

void Controller::hide(bool hide) {
	classifier_.disable(hide);
	bottomLine_.disable(hide);
	name_.disable(hide);
	bg_.disable(hide);
}

bool Controller::hidden() const {
	return name_.disabled();
}

void Controller::size(Vec2f size) {
	auto toff = (size.y - gui().font().height()) / 2;
	auto tpos = Vec {toff, classifierWidth + std::max(toff, classifierWidth)};
	name_.change()->position = tpos;

	auto start = Vec{classifierWidth / 2.f, 0.f};
	auto end = Vec{start.x, size.y};
	classifier_.change()->points = {start, end};

	bottomLine_.change()->points = {{0.f, size.y}, size};
	bg_.change()->size = size;
}

// Button
Button::Button(Panel& panel, Vec2f pos, std::string_view name) :
		Controller(panel, pos, name) {

	bgColor_ = {panel.context(), rvg::colorPaint(colors::bg)};
}

const rvg::Paint& Button::classPaint() const {
	return panel().paints().buttonClass;
}

void Button::mouseOver(bool mouseOver) {
	hovered_ = mouseOver;
	if(mouseOver) {
		bgColor_.paint(rvg::colorPaint(colors::bgHover));
	} else {
		bgColor_.paint(rvg::colorPaint(colors::bg));
	}
}

Widget* Button::mouseButton(const MouseButtonEvent& ev) {
	if(ev.button == MouseButton::left) {
		if(ev.pressed) {
			pressed_ = true;
			bgColor_.paint(rvg::colorPaint(colors::bgActive));
		} else if(pressed_) {
			auto col = hovered_ ? colors::bgHover : colors::bgActive;
			bgColor_.paint(rvg::colorPaint(col));
			pressed_ = false;
			if(hovered_ && onClick) {
				onClick();
			}
		}
	}

	return this;
}

// Text
Textfield::Textfield(Panel& panel, Vec2f pos, std::string_view
		name, std::string_view start) : Controller(panel, pos, name) {

	auto height = panel.rowHeight() - 4;
	auto width = size().x - panel.nameWidth() - 8;
	auto tpos = position() + Vec{panel.nameWidth() + 4, 2};
	auto bounds = Rect2f {tpos, Vec{width, height}};
	textfield_.emplace(panel.gui(), bounds, start, panel.styles().textfield);
}

void Textfield::size(Vec2f) {
	// TODO
}

const rvg::Paint& Textfield::classPaint() const {
	return panel().paints().textClass;
}

void Textfield::nameWidth(float width) {
	auto pos = textfield_->position();
	textfield_->position({position().x + width, pos.y});
}

void Textfield::hide(bool hide) {
	textfield_->hide(hide);
	Controller::hide(hide);
}

void Textfield::draw(const DrawInstance& di) const {
	Controller::draw(di);
	textfield_->draw(di);
}

void Textfield::refreshTransform() {
	textfield_->refreshTransform();
	Controller::refreshTransform();
}

Widget* Textfield::key(const KeyEvent& ev) {
	if(focus_) {
		return textfield_->key(ev);
	}

	return nullptr;
}

Widget* Textfield::textInput(const TextInputEvent& ev) {
	if(focus_) {
		return textfield_->textInput(ev);
	}

	return nullptr;
}

Widget* Textfield::mouseButton(const MouseButtonEvent& ev) {
	if(mouseOver_) {
		if(!focus_) {
			textfield_->focus(true);
			focus_ = true;
		}

		return textfield_->mouseButton(ev);
	} else if(focus_) {
		textfield_->focus(false);
		focus_ = false;
	}

	return this;
}

Widget* Textfield::mouseMove(const MouseMoveEvent& ev) {
	if(textfield_->contains(ev.position)) {
		if(!mouseOver_) {
			textfield_->mouseOver(true);
			mouseOver_ = true;
		}

		textfield_->mouseMove(ev);
	} else if(mouseOver_) {
		mouseOver_ = false;
		textfield_->mouseOver(false);
	}

	return this;
}

void Textfield::mouseOver(bool gained) {
	if(!gained && mouseOver_) {
		mouseOver_ = false;
		textfield_->mouseOver(false);
	}
}

void Textfield::focus(bool gained) {
	if(!gained && focus_) {
		focus_ = false;
		textfield_->focus(false);
	}
}

void Textfield::position(Vec2f position) {
	auto off = textfield_->position() - this->position();
	textfield_->position(position + off);
	Controller::position(position);
	textfield_->intersectScissor(scissor());
}

void Textfield::intersectScissor(const Rect2f& scissor) {
	Controller::intersectScissor(scissor);
	textfield_->intersectScissor(this->scissor());
}

// Label
Label::Label(Panel& panel, Vec2f pos, std::string_view name,
		std::string_view label) : Controller(panel, pos, name) {

	auto y = ((panel.rowHeight() - gui().font().height()) / 2);
	label_ = {context(), label, gui().font(), {panel.nameWidth() + 4, y}};
}

const rvg::Paint& Label::classPaint() const {
	return panel().paints().labelClass;
}

void Label::label(std::string_view label) {
	label_.change()->utf8(label);
}

void Label::hide(bool hide) {
	label_.disable(hide);
	Controller::hide(hide);
}

void Label::draw(const DrawInstance& di) const {
	Controller::draw(di);

	panel_.paints().name.bind(di);
	label_.draw(di);
}

// Checkbox
Checkbox::Checkbox(Panel& panel, Vec2f pos, std::string_view name) :
		Controller(panel, pos, name) {

	auto size = panel.rowHeight() - 8;
	auto cpos = position() + Vec{panel.nameWidth() + 4, 4};
	auto bounds = Rect2f{cpos, Vec{size, size}};
	checkbox_.emplace(panel.gui(), bounds);
}

const rvg::Paint& Checkbox::classPaint() const {
	return panel().paints().checkboxClass;
}

void Checkbox::hide(bool hide) {
	checkbox_->hide(hide);
	Controller::hide(hide);
}

Widget* Checkbox::mouseButton(const MouseButtonEvent& ev) {
	if(ev.pressed && ev.button == MouseButton::left) {
		checkbox_->toggle();
	}

	return this;
}

void Checkbox::draw(const DrawInstance& di) const {
	Controller::draw(di);
	checkbox_->draw(di);
}
void Checkbox::refreshTransform() {
	Controller::refreshTransform();
	checkbox_->refreshTransform();
}

void Checkbox::position(Vec2f position) {
	auto off = checkbox_->position() - this->position();
	Controller::position(position);
	checkbox_->position(position + off);
	checkbox_->intersectScissor(scissor());
}

void Checkbox::intersectScissor(const Rect2f& scissor) {
	Controller::intersectScissor(scissor);
	checkbox_->intersectScissor(this->scissor());
}

// Panel
Panel::Panel(Gui& gui, const nytl::Rect2f& bounds, float rowHeight) :
		ContainerWidget(gui, bounds), rowHeight_(rowHeight) {

	if(rowHeight_ == autoSize) {
		rowHeight_ = gui.font().height() + 10;
	}

	auto size = bounds.size;
	if(size.x == autoSize) {
		size.x = 350;
	}

	if(size.y == autoSize) {
		size.y = rowHeight_;
	}

	if(size != bounds.size) {
		Widget::size(size);
	}

	// paints
	using namespace colors;
	paints_.name = {context(), rvg::colorPaint(name)};
	paints_.line = {context(), rvg::colorPaint(line)};
	paints_.buttonClass = {context(), rvg::colorPaint(button)};
	paints_.labelClass = {context(), rvg::colorPaint(label)};
	paints_.textClass = {context(), rvg::colorPaint(text)};
	paints_.rangeClass = {context(), rvg::colorPaint(range)};
	paints_.checkboxClass = {context(), rvg::colorPaint(checkbox)};

	paints_.bg = {context(), rvg::colorPaint(bg)};
	paints_.bgHover = {context(), rvg::colorPaint(bgHover)};
	paints_.bgActive = {context(), rvg::colorPaint(bgActive)};

	paints_.bgWidget = {context(), rvg::colorPaint(bgWidget)};

	// styles
	styles_.textfield = gui.styles().textfield;
	styles_.textfield.bg = &paints_.bgWidget;

	// toggle button
	auto btnBounds = Rect2f{position(), {width(), rowHeight_}};
	auto btn = std::make_unique<LabeledButton>(gui, btnBounds,
		"Toggle Controls");
	toggleButton_ = btn.get();
	widgets_.push_back(std::move(btn));
	toggleButton_->onClick = [&](auto&) { this->toggle(); };
}

void Panel::hide(bool hide) {
	ContainerWidget::hide(hide);
}

bool Panel::hidden() const {
	return toggleButton_->hidden();
}

void Panel::size(Vec2f size) {
	// TODO
	ContainerWidget::size(size);
}

Widget& Panel::add(std::unique_ptr<Widget> ctrl) {
	dlg_assert(!widgets_.empty());

	auto& ret = *ctrl;
	widgets_.emplace(widgets_.end() - 1, std::move(ctrl));

	auto s = size();
	s.y += ret.size().y;
	Widget::size(s);

	auto p = position();
	toggleButton_->position({p.x, p.y + s.y - rowHeight_});

	return ret;
}

void Panel::open(bool open) {
	if(open_ == open) {
		return;
	}

	open_ = open;
	auto size = this->size();

	if(open_) {
		size.y = 0.f;
		for(auto& w : widgets_) {
			size.y += w->size().y;
		}
	} else {
		size.y = rowHeight_;
	}

	for(auto& w :widgets_) {
		if(w.get() != toggleButton_) {
			w->hide(!open);
		}
	}

	auto y = size.y - rowHeight_;
	toggleButton_->position({position().x, y});
	Widget::size(size);
}

void Panel::refreshLayout() {
	auto y = position().y;
	for(auto& w : widgets_) {
		w->position({w->position().x, y});

		// NOTE: this is a hack. Should be done better
		auto folder = dynamic_cast<Folder*>(w.get());
		if(folder) {
			folder->refreshLayout();
		}

		// /hack
		y += w->size().y;
	}

	if(y != size().y) {
		Widget::size({size().x, y - position().y});
	}
}

// Folder
Folder::Folder(Panel& panel, Vec2f pos, std::string_view name) :
	ContainerWidget(panel.gui(), {pos,
			{panel.position().x + panel.width() - pos.x, panel.rowHeight()}}),
		panel_(panel) {

	auto btnBounds = Rect2f{position(), {size().x, panel.rowHeight()}};
	auto btn = std::make_unique<LabeledButton>(gui(), btnBounds, name);
	button_ = btn.get();
	add(std::move(btn));
	button_->onClick = [&](auto&) { this->toggle(); };
}

void Folder::hide(bool hide) {
	ContainerWidget::hide(hide);
}

bool Folder::hidden() const {
	return button_->hidden();
}

Widget& Folder::add(std::unique_ptr<Widget> widget) {
	auto& ret = *widget;
	widgets_.emplace_back(std::move(widget));

	auto s = size();
	s.y += ret.size().y;
	Widget::size(s);

	// TODO will call this->refreshLayout and size again
	panel().refreshLayout();

	return ret;
}

void Folder::open(bool open) {
	if(open_ == open) {
		return;
	}

	open_ = open;
	auto size = this->size();

	if(open_) {
		size.y = 0.f;
		for(auto& w : widgets_) {
			size.y += w->size().y;
		}
	} else {
		size.y = panel().rowHeight();
	}

	for(auto& w : widgets_) {
		if(w.get() != button_) {
			w->hide(!open);
		}
	}

	// TODO: somewhat redundant
	Widget::size(size);
	panel().refreshLayout();
}

void Folder::refreshLayout() {
	auto y = position().y;
	for(auto& w : widgets_) {
		w->position({w->position().x, y});

		// NOTE: this is a hack. Should be done better
		auto folder = dynamic_cast<Folder*>(w.get());
		if(folder) {
			folder->refreshLayout();
		}
		// /hack

		y += w->size().y;
	}

	if(open_ && y != size().y) {
		Widget::size({size().x, y - position().y});
	}
}

} // namespace vui::dat
