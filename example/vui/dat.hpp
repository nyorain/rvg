/// Simple widgets classes modeled after googles dat.gui
/// https://github.com/dataarts/dat.gui/blob/master/API.md

#include <rvg/context.hpp>
#include <rvg/text.hpp>
#include <rvg/shapes.hpp>

#include "widget.hpp"
#include "textfield.hpp"
#include "checkbox.hpp"

namespace vui::dat {

class Panel;

class Controller : public Widget {
public:
	Controller(Panel&, float yoff, std::string_view name,
		const rvg::Paint& classPaint);

	// TODO
	void hide(bool) override {}
	bool hidden() const override { return false; }
	void size(Vec2f) override {}

	void draw(const rvg::DrawInstance&) const override;
	auto yoff() const { return yoff_; }

protected:
	const Panel& panel_;
	rvg::Shape classifier_;
	rvg::Shape bottomLine_;
	rvg::Text name_;
	const rvg::Paint& classifierPaint_;
	float yoff_;
};

class Textfield : public Controller {
public:
	Textfield(Panel&, float yoff, std::string_view name,
		std::string_view start = "");

	Widget* key(const KeyEvent&) override;
	Widget* textInput(const TextInputEvent&) override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseMove(const MouseMoveEvent&) override;
	void mouseOver(bool) override;
	void focus(bool) override;

	void draw(const DrawInstance&) const override;
	void refreshTransform() override;

protected:
	std::optional<vui::Textfield> textfield_;
	bool focus_ {};
	bool mouseOver_ {};
};

class Button : public Controller {
public:
	std::function<void()> onClick;

public:
	Button(Panel& panel, float yoff, std::string_view name);

	void mouseOver(bool) override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	void draw(const DrawInstance&) const override;

protected:
	rvg::RectShape bg_;
	rvg::Paint bgColor_;
	bool hovered_ {};
	bool pressed_ {};
};

class Label : public Controller {
public:
	Label(Panel& panel, float yoff, std::string_view name,
		std::string_view label);
	void label(std::string_view label);
	void draw(const DrawInstance&) const override;

protected:
	rvg::Text label_;
};

class Checkbox : public Controller {
public:
	Checkbox(Panel& panel, float yoff, std::string_view name);
	Widget* mouseButton(const MouseButtonEvent&) override;
	void draw(const DrawInstance&) const override;
	void refreshTransform() override;

protected:
	std::optional<vui::Checkbox> checkbox_;
};

class Folder : public Widget {
};

class Panel : public Widget {
public:
	Panel(Gui& gui, const nytl::Rect2f& bounds,
		float rowHeight = autoSize);

	template<typename T, typename... Args>
	T& create(Args&&... args) {
		auto yoff = rowHeight() * widgets_.size();
		auto ctrl = std::make_unique<T>(*this, yoff,
			std::forward<Args>(args)...);
		auto& ret = *ctrl;
		add(std::move(ctrl));
		return ret;
	}

	void draw(const DrawInstance& di) const override;

	bool hidden() const override { return false; }
	void size(Vec2f) override {
		throw std::runtime_error("vui::dat::Panel: Cannot be resized");
	}
	void hide(bool) override {
		throw std::runtime_error("vui::dat::Panel: Cannot be hidden");
	}

	using Widget::size;

	Widget* mouseButton(const MouseButtonEvent&) override;
	Widget* mouseMove(const MouseMoveEvent&) override;
	Widget* key(const KeyEvent&) override;
	Widget* textInput(const TextInputEvent&) override;
	void focus(bool focus) override;
	void mouseOver(bool mouseOver) override;
	void refreshTransform() override;

	float rowHeight() const { return rowHeight_; }
	float width() const { return size().x; }

	float nameWidth() const { return nameWidth_; }
	void nameWidth(float);

	void open(bool open);
	bool open() const { return open_; }
	void toggle() { open(!open_); }

	// TODO
	// void remove(Widget&);

	// for controllers
	const auto& paints() const { return paints_; }
	const auto& styles() const { return styles_; }

protected:
	void add(std::unique_ptr<Widget>);

protected:
	Widget* mouseOver_ {};
	Widget* focus_ {};

	std::vector<std::unique_ptr<Widget>> widgets_;
	float rowHeight_ {};
	float nameWidth_ {150.f};
	bool open_ {true};

	rvg::RectShape bg_;

	struct {
		rvg::RectShape bg;
		rvg::Paint paint;
		bool pressed {};
		bool hovered {};
		rvg::Text text;
	} toggleButton_;

	struct {
		rvg::Paint bg;
		rvg::Paint bgHover;
		rvg::Paint bgActive;

		rvg::Paint name;
		rvg::Paint line;
		rvg::Paint buttonClass;
		rvg::Paint textClass;
		rvg::Paint labelClass;
		rvg::Paint rangeClass;
		rvg::Paint checkboxClass;

		rvg::Paint bgWidget;
	} paints_;

	struct {
		TextfieldStyle textfield;
	} styles_;
};

// TODO
class RangeController : public Controller {
protected:
	rvg::RectShape bg_;
	rvg::RectShape fg_;
	rvg::Paint mainPaint_;

	rvg::RectShape valueBg_;
	rvg::Text value_;
	rvg::Paint valuePaint_;
};

} // namespace vui::dat
