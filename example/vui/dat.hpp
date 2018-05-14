/// Simple widgets classes modeled after googles dat.gui
/// https://github.com/dataarts/dat.gui/blob/master/API.md

#include <rvg/context.hpp>
#include <rvg/text.hpp>
#include <rvg/shapes.hpp>

#include "widget.hpp"
#include "textfield.hpp"
#include "checkbox.hpp"
#include "container.hpp"
#include "button.hpp"

// NOTES
// - make color classifiers optional. dat.gui has them but they
//   are not really needed.
// - currently code duplication between folder and panel. Furthermore
//   ugly refreshLayout hack
// - missing intersectScissors. Needed here at all? test it
// - reenable todo in ContainerWidget::position. Breaks nested folders
// - still some clipping issues (probably related to scissor)

namespace vui::dat {

class Panel;

class Controller : public Widget {
public:
	Controller(Panel&, Vec2f, std::string_view name);

	virtual const rvg::Paint& classPaint() const = 0;
	virtual const rvg::Paint& bgPaint() const;

	virtual void nameWidth(float) {}
	auto& panel() const { return panel_; }

	void hide(bool) override;
	bool hidden() const override;
	void draw(const rvg::DrawInstance&) const override;
	void size(Vec2f) override;
	using Widget::size;

protected:
	Panel& panel_;
	rvg::RectShape bg_;
	rvg::Shape classifier_;
	rvg::Shape bottomLine_;
	rvg::Text name_;
};

class Button : public Controller {
public:
	std::function<void()> onClick;

public:
	Button(Panel&, Vec2f, std::string_view name);

	const rvg::Paint& classPaint() const override;
	const rvg::Paint& bgPaint() const override { return bgColor_; }

	void mouseOver(bool) override;
	Widget* mouseButton(const MouseButtonEvent&) override;

protected:
	rvg::Paint bgColor_;
	bool hovered_ {};
	bool pressed_ {};
};


class Textfield : public Controller {
public:
	Textfield(Panel&, Vec2f, std::string_view name,
		std::string_view start = "");

	void size(Vec2f) override;
	using Widget::size;

	void position(Vec2f position) override;
	void intersectScissor(const Rect2f& scissor) override;
	using Controller::position;

	const rvg::Paint& classPaint() const override;
	void nameWidth(float) override;
	void hide(bool hide) override;

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

class Label : public Controller {
public:
	Label(Panel&, Vec2f, std::string_view name,
		std::string_view label);

	const rvg::Paint& classPaint() const override;
	void hide(bool hide) override;
	void label(std::string_view label);
	void draw(const DrawInstance&) const override;

protected:
	rvg::Text label_;
};

class Checkbox : public Controller {
public:
	Checkbox(Panel&, Vec2f, std::string_view name);

	void hide(bool hide) override;
	const rvg::Paint& classPaint() const override;
	Widget* mouseButton(const MouseButtonEvent&) override;
	void draw(const DrawInstance&) const override;

	void refreshTransform() override;
	void position(Vec2f position) override;
	void intersectScissor(const Rect2f& scissor) override;
	using Controller::position;

protected:
	std::optional<vui::Checkbox> checkbox_;
};

class Panel : public ContainerWidget {
public:
	Panel(Gui& gui, const nytl::Rect2f& bounds, float rowHeight = autoSize);

	template<typename T, typename... Args>
	T& create(Args&&... args) {
		auto pos = position();
		pos.y += size().y - rowHeight();
		auto ctrl = std::make_unique<T>(*this, pos,
			std::forward<Args>(args)...);
		auto& ret = *ctrl;
		add(std::move(ctrl));
		return ret;
	}

	void hide(bool) override;
	bool hidden() const override;
	void size(Vec2f) override;

	using Widget::size;

	float rowHeight() const { return rowHeight_; }
	float width() const { return size().x; }

	float nameWidth() const { return nameWidth_; }
	void nameWidth(float);

	void open(bool open);
	bool open() const { return open_; }
	void toggle() { open(!open_); }

	void refreshLayout();

	// TODO
	// void remove(Widget&);

	// for controllers
	const auto& paints() const { return paints_; }
	const auto& styles() const { return styles_; }

protected:
	Widget& add(std::unique_ptr<Widget>) override;

protected:
	float rowHeight_ {};
	float nameWidth_ {150.f};
	bool open_ {true};
	LabeledButton* toggleButton_ {};

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

class Folder : public ContainerWidget {
public:
	static constexpr auto xoff = 5.f;

public:
	Folder(Panel& panel, Vec2f, std::string_view name);

	void open(bool);
	bool open() const { return open_; }
	void toggle() { open(!open_); }

	void hide(bool) override;
	bool hidden() const override;
	void size(Vec2f) override {} // TODO
	using Widget::size;

	template<typename T, typename... Args>
	T& create(Args&&... args) {
		auto pos = position();
		pos.x += xoff;
		pos.y += size().y;
		auto ctrl = std::make_unique<T>(panel_, pos,
			std::forward<Args>(args)...);
		auto& ret = *ctrl;
		add(std::move(ctrl));
		return ret;
	}

	void refreshLayout();
	auto& panel() const { return panel_; }

protected:
	Widget& add(std::unique_ptr<Widget> widget) override;

protected:
	Panel& panel_;
	LabeledButton* button_;
	bool open_ {true};
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
