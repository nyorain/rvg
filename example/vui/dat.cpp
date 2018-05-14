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

constexpr auto classifierWidth = 3.f;

// Controller
Controller::Controller(Panel& panel, float yoff,
	std::string_view name, const rvg::Paint& classPaint) :
		Widget(panel.gui(), {
			panel.position() + Vec {0, yoff},
			{panel.width(), panel.rowHeight()}
		}), panel_(panel), classifierPaint_(classPaint), yoff_(yoff) {

	auto& ctx = panel.context();
	auto height = panel.rowHeight();

	auto pos = nytl::Vec2f {0.f, yoff};
	auto off = (height - panel.gui().font().height()) / 2;
	auto tpos = pos;
	tpos.y += off;
	tpos.x += classifierWidth + std::max(off, classifierWidth);
	name_ = {ctx, name, panel.gui().font(), tpos};

	auto start = pos;
	start.x += classifierWidth / 2.f;
	auto end = start;
	end.y += panel.rowHeight();
	classifier_ = {ctx, {start, end}, {false, 3.f}};

	start = pos;
	start.y += panel.rowHeight();
	end = start;
	end.x += panel.width();
	bottomLine_ = {ctx, {start, end}, {false, 1.f}};
}

void Controller::draw(const rvg::DrawInstance& di) const {
	// TODO
	// Widget::bindState(di);

	classifierPaint_.bind(di);
	classifier_.stroke(di);

	panel_.paints().name.bind(di);
	name_.draw(di);

	panel_.paints().line.bind(di);
	bottomLine_.stroke(di);
}

// Button
Button::Button(Panel& panel, float yoff, std::string_view name) :
		Controller(panel, yoff, name, panel.paints().buttonClass) {
	auto size = nytl::Vec {panel.width(), panel.rowHeight()};
	bg_ = {panel.context(), {0.f, yoff}, size, {true, 0.f}};
	bgColor_ = {panel.context(), rvg::colorPaint(colors::bg)};
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

void Button::draw(const DrawInstance& di) const {
	bgColor_.bind(di);
	bg_.fill(di);
	Controller::draw(di);
}

// Text
Textfield::Textfield(Panel& panel, float yoff, std::string_view
	name, std::string_view start) : Controller(panel, yoff, name,
		panel.paints().textClass) {

	nytl::unused(start); // TODO
	auto pos = panel.position();
	pos.y += yoff + 2;
	pos.x += panel.nameWidth() + 4;
	auto height = panel.rowHeight() - 4;
	auto width = panel.width() - panel.nameWidth() - 8;
	auto bounds = Rect2f {pos, Vec{width, height}};
	textfield_.emplace(panel.gui(), bounds, panel.styles().textfield);
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

// Label
Label::Label(Panel& panel, float yoff, std::string_view name,
	std::string_view label) : Controller(panel, yoff, name,
		panel.paints().labelClass) {

	auto pos = panel.position();
	pos.x += panel.nameWidth() + 4;
	label_ = {context(), label, gui().font(), pos};
}

void Label::label(std::string_view label) {
	label_.change()->utf8(label);
}

void Label::draw(const DrawInstance& di) const {
	Controller::draw(di);

	panel_.paints().name.bind(di);
	label_.draw(di);
}

// Checkbox
Checkbox::Checkbox(Panel& panel, float yoff, std::string_view name) :
		Controller(panel, yoff, name, panel.paints().checkboxClass) {

	auto pos = panel.position();
	pos.y += yoff + 4;
	pos.x += panel.nameWidth() + 4;
	auto size = panel.rowHeight() - 8;
	auto bounds = Rect2f {pos, Vec{size, size}};
	checkbox_.emplace(panel.gui(), bounds);
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

// Panel
Panel::Panel(Gui& gui, const nytl::Rect2f& bounds, float rowHeight) :
		Widget(gui, bounds), rowHeight_(rowHeight) {

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

	constexpr auto toggleText = "Toggle Controls";
	auto tw = gui.font().width(toggleText);
	auto tpos = nytl::Vec {
		(width() - tw) / 2,
		(rowHeight_ - gui.font().height()) / 2
	};
	toggleButton_.text = {context(), toggleText, gui.font(), tpos};
	toggleButton_.bg = {context(), {}, {width(), rowHeight_}, {true, 0.f}};
	toggleButton_.paint = {context(), rvg::colorPaint(colors::bg)};

	bg_ = {context(), {}, size, {true, 0.f}};

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
}

void Panel::draw(const DrawInstance& di) const {
	Widget::bindState(di);

	paints_.bg.bind(di);
	bg_.fill(di);

	for(auto& ctrl : widgets_) {
		Widget::bindState(di);
		ctrl->draw(di);
	}

	Widget::bindState(di);
	toggleButton_.paint.bind(di);
	toggleButton_.bg.fill(di);

	paints_.name.bind(di);
	toggleButton_.text.draw(di);
}

void Panel::add(std::unique_ptr<Widget> ctrl) {
	widgets_.emplace_back(std::move(ctrl));

	auto y = widgets_.size() * rowHeight_;
	auto s = size();
	s.y += rowHeight_;
	Widget::size(s);

	bg_.change()->size = s;

	auto ty = y + (rowHeight_ - gui().font().height()) / 2;
	toggleButton_.text.change()->position.y = ty;
	toggleButton_.bg.change()->position.y = y;
}

Widget* Panel::mouseButton(const MouseButtonEvent& ev) {
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
	} else if(toggleButton_.hovered || toggleButton_.pressed) {
		if(ev.button == MouseButton::left) {
			if(ev.pressed) {
				toggleButton_.pressed = true;
				toggleButton_.paint.paint(rvg::colorPaint(colors::bgActive));
			} else if(toggleButton_.pressed) {
				auto col = colors::bg;
				if(toggleButton_.hovered) {
					col = colors::bgHover;
					toggle();
				}

				toggleButton_.pressed = false;
				toggleButton_.paint.paint(rvg::colorPaint(col));
			}
		}
	}

	return this;
}

Widget* Panel::mouseMove(const MouseMoveEvent& ev) {
	auto pos = ev.position - position();
	auto oor =
		std::clamp(pos.x, 0.f, size().x) != pos.x ||
		std::clamp(pos.y, 0.f, size().y) != pos.y;

	auto id = unsigned(pos.y / rowHeight_);
	if(!open_ || id == widgets_.size()) {
		toggleButton_.hovered = true;
		if(!toggleButton_.pressed) {
			toggleButton_.paint.paint(rvg::colorPaint(colors::bgHover));
		}
	}

	if((oor || id >= widgets_.size())) {
		if(mouseOver_) {
			mouseOver_->mouseOver(false);
			mouseOver_ = nullptr;
		}
	} else if(open_) {
		auto& ctrl = widgets_[id];
		if(ctrl.get() != mouseOver_) {
			if(mouseOver_) {
				mouseOver_->mouseOver(false);
				mouseOver_ = nullptr;
			}

			ctrl->mouseOver(true);
			mouseOver_ = ctrl.get();
		}

		ctrl->mouseMove(ev);
	}

	if(mouseOver_ && toggleButton_.hovered) {
		toggleButton_.hovered = false;
		if(!toggleButton_.pressed) {
			toggleButton_.paint.paint(rvg::colorPaint(colors::bg));
		}
	}

	return this;
}

Widget* Panel::key(const KeyEvent& ev) {
	if(focus_) {
		return focus_->key(ev);
	}

	return nullptr;
}

Widget* Panel::textInput(const TextInputEvent& ev) {
	if(focus_) {
		return focus_->textInput(ev);
	}

	return nullptr;
}

void Panel::focus(bool gained) {
	if(!gained && focus_) {
		focus_->focus(false);
		focus_ = nullptr;
	}
}

void Panel::mouseOver(bool gained) {
	if(!gained) {
		if(mouseOver_) {
			mouseOver_->mouseOver(false);
			mouseOver_ = nullptr;
		} else if(toggleButton_.hovered) {
			toggleButton_.hovered = false;
			if(!toggleButton_.pressed) {
				toggleButton_.paint.paint(rvg::colorPaint(colors::bg));
			}
		}
	}
}

void Panel::refreshTransform() {
	for(auto& ctrl :widgets_) {
		ctrl->refreshTransform();
	}

	Widget::refreshTransform();
}

void Panel::open(bool open) {
	if(open_ == open) {
		return;
	}

	open_ = open;
	auto size = this->size();

	if(open_) {
		size.y = (widgets_.size() + 1) * rowHeight_;
	} else {
		size.y = rowHeight_;
	}

	bg_.change()->size = size;

	auto ty = size.y - rowHeight_  + (rowHeight_ - gui().font().height()) / 2;
	toggleButton_.text.change()->position.y = ty;
	toggleButton_.bg.change()->position.y = size.y - rowHeight_;

	Widget::size(size);
}

} // namespace vui::dat
