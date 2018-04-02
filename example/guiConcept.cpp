// sources
// https://github.com/nothings/stb/blob/master/stb_textedit.h

struct MouseMoveEvent {
	Vec2f position;
};

struct MouseButtonEvent {
	bool pressed;
	unsigned int button;
	Vec2f position
};

struct KeyEvent {
	unsigned int key;
	bool pressed;
};

struct TextInputEvent {
	const char* utf8;
};

class Widget {
public:
	Rect2f bounds;

public:
	virtual void mouseMove(const MouseMoveEvent&) {}
	virtual void mouseButton(const MouseButtonEvent&) {}
	virtual void key(const KeyEvent&) {}
	virtual void textInput(const TextInputEvent&) {}

	virtual void focus(bool) {}
	virtual void mouseOver(bool) {}

	virtual void draw(vk::CommandBuffer) {}
	virtual void update(double delta) {}
	virtual bool updateDevice() {}

	void registerUpdate();
	void registerUpdateDevice();
};

class Gui {
public:
	Callback<void(Gui&, std::string)> onCopy;
	Callback<void(Gui&, Widget&)> onPasteRequest;

public:
	const Context& context() const;
	const Font& defaultFont() const;

	struct {
		Paint buttonLabel;
		Paint buttonBackground;
		Paint textfieldBackground;
		Paint textfieldText;
	} paints;
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
	Polygon background_;
	Text label_;
};

class Textfield : public Widget {
public:
	Callback<void(Textfield&)> onChange;
	Callback<void(Textfield&)> onEnter;
	std::string text;

public:
	Textfield(Gui&);

	void mouseButton(const MouseButtonEvent&) override;
	void key(const KeyEvent&) override;
	void focus(bool gained) override;
	void mouseOver(bool gained) override;
	void textInput(const TextInputEvent&) override;

	void draw(vk::CommandBuffer) override;
	void update(double delta) override;
	bool updateDevice() override;

protected:
	Polygon background_;
	Polygon cursor_;
	Text label_;
};

class TextPopup : public Widget {
public:
	std::string text;

public:
	TextPopup(Gui&);

	void draw(vk::CommandBuffer) override;
	bool updateDevice() override;

protected:
	Polygon background_;
	Text text_;
};

class Checkbox : public Widget {};

class Dropdown : public Widget {
protected:
	class Popup : public Widget {
	};
};


// TODO: later, v0.2
class Window {
public:
	void mouseMove(const MouseMoveEvent&);
	void mouseButton(const MouseButtonEvent&);
	void key(const KeyEvent&);
	void textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

protected:
	std::vector<std::unique_ptr<Widget>> widgets_;
};




// #1
Gui gui {};

Button button1 {gui, "button1"};
button1.onClick += ...;

Button button2 {gui, "button2"};
button2.onClick += ...;


// #2
Gui gui {};

auto& button1 = gui.create<Button>("button1");
auto& button2 = gui.create<Button>("button1");


// Problems:
//  - ownership (see above)
//  - who manages bounds of widget? most widgets laid out in containers that
//    will manage their bounds but e.g. text popup might want to adapt
//    its size to the displayed text.
//    - popups (text or from dropdown menus) should probably not be
//      used in layouts either. We could simply make say a popup
//      is not a widget
//  - when to call updateDevice? who decides? Don't call it every frame,
//    only when there is something to update.
//    - answer: when caller changes something (like a relevant public variable),
//      he has to call widget.registerUpdateDevice or registerUpdate.
//      When the change comes internally (e.g. from a textbox processing
//      a text input event) the widget handles it automatically.
//  - z order: what about multiple widgets at the same position?
//    how to handle drawing & events?
//    - could be left undefined except for those special (non-?) widgets like
//      windows and popups

// Ownership: for now, let's go with #2, i.e. owned trees
class Gui {
public:
	Callback<void(Gui&, std::string)> onCopy;
	Callback<void(Gui&, Widget&)> onPasteRequest;

	struct {
		Paint buttonLabel;
		Paint buttonBackground;
		Paint textfieldBackground;
		Paint textfieldText;
	} paints;

public:
	Gui(Context& context);

	void mouseMove(const MouseMoveEvent&);
	void mouseButton(const MouseButtonEvent&);
	void key(const KeyEvent&);
	void textInput(const TextInputEvent&);
	void focus(bool gained);
	void mouseOver(bool gained);

	void update(double delta);
	bool updateDevice();
	void draw(vk::CommandBuffer);

	Widget* mouseOver();
	Widget* focus();

	const Context& context() const;
	const Font& defaultFont() const;

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
	std::vector<std::unique_ptr<Widget>> widgets_;
	std::vector<Widget*> update_;
	std::vector<Widget*> updateDevice_;
	Context& context_;
};

class Widget : public nytl::NonMovable {
public:
	Gui& gui;
	Rect2f bounds;

public:
	Widget(Gui&);

	virtual void mouseMove(const MouseMoveEvent&) {}
	virtual void mouseButton(const MouseButtonEvent&) {}
	virtual void key(const KeyEvent&) {}
	virtual void textInput(const TextInputEvent&) {}

	virtual void focus(bool) {}
	virtual void mouseOver(bool) {}

	virtual void draw(vk::CommandBuffer) {}
	virtual void update(double delta) {}
	virtual bool updateDevice() {}

	// registers wiget for update/updateDevice in next call at gui
	void registerUpdate();
	void registerUpdateDevice();
};

// problem: how to handle keycodes/mousebuttons?

// -- solution #1 --
// always use unsigned int, register special codes from backend
// the gui object internally holds a table
gui.registerKeyCode(Key::enter, (unsigned int) backendEnterKeycode);

// when passing event to gui, just use the backends keycode
gui.keyEvent({(unsigned int) backendKeycode, ...});

// in widgets, received keycodes are checked against special keys
if(gui.keyMatches(ev.keycode, Key::enter)) { ... }


// -- option #2 --
// Use a fixed enum like most backend libraries already do
// Probably best to just use the linux enumeration (as ny does)
enum class Keycode {
	...
};

// when passing events to the gui the keycode have to be transformed
// if they already use the values from linux.h, a cast is sufficient
gui.keyEvent({convertKey(backendKeycode), ...});

// widgets can simply check the keycodes
if(ev.keycode == Keycode::enter) { ... }


// --- solution ---
// go with solution #2 for now. Makes it easier for anyone and better a
// static enumeration that a dynamic large table in the gui object





// problem: how to handle different paints for different widget states.
// example: button normal/hovered/pressed. All different states might
//   render the button in different color.

// -- option #1 --
// give every widget its own paint (uniform buffer + descriptor set).
// pro: no cmd buffer rerecording when state changes
// contra: much more resources and memory used

// -- option #2 --
// allocate those different paints once and share them between widgets
// pro/cons opposite of above

// -- option #3 (?) --
// something in between should be possible. Allocate the paints uniform buffers
// statically but bind them dynamically, i.e. give every widget its own
// descriptor set. Would need modifications to vgv though


vgv::PaintBuffer paint(ctx, color);
vgv::PaintBinding paintBinding(ctx, paint);
paintBinding.buffer(otherPaint);

paintBinding.bind(ctx, cmdb);
polygon.fill(ctx, cmdb);





















// dumping some vgv concept here

/// The lowest shape abstraction. Simply draws a set of points as triangles.
/// Does only hold its device state, isn't doubled buffered (host -> host).
/// Can use an indirect drawing command, allowing the number of points to
/// change without a need for rerecording.
class PointBuffer {
public:
	PointBuffer() = default;
	PointBuffer(Context&, bool indirect = false);

	/// Updates the device with the given set of points.
	/// Drawing will use undefined uv coordinates.
	/// Returns whether if a resource was recreated and a command buffer
	/// rerecord is needed.
	bool updateDevice(Context&, Span<Vec2f> points);

	/// Updates the device with the given set of points and its uv coords.
	/// Returns whether if a resource was recreated and a command buffer
	/// rerecord is needed.
	bool updateDevice(Context&, Span<Vec2f> points, Span<Vec2f> uvs);

	/// Records a draw command for the stored point into the given command
	/// buffer. Has No effect if no points were stored yet.
	/// Stencil specifies whether a stencil buffer should be used.
	void draw(Context&, vk::CommandBuffer, PolygonMode mode,
		bool stencil = false);

protected:
	vpp::BufferRange verts_ {};
	bool indirect_ {};
	size_t drawCount_ {};
};

/// A simple polygon.
class Polygon {
public:
	std::vector<Vec2f> points;
	bool useStencil {};

public:
	void bakeStroke(float width, LineCap, LineJoin);
	bool updateDevice(DrawMode);

	void fill(Context&, vk::CommandBuffer, bool useStencil = true);
	void stroke(Context&, vk::CommandBuffer, bool useStencil = true);

protected:
	PointBuffer fill_;
	PointBuffer fillAntilias_;

	std::vector<Vec2f> strokeCache_;
	PointBuffer stroke_;
};

class Rect {
public:
	Vec2f pos;
	Vec2f size;

public:
	void bakeStroke(float width, LineCap, LineJoin);
	bool updateDevice(DrawMode);
};













class FillPolygon {
public:
	Polygon() = default;
	Polygon(Context&, bool indirect = false);

	bool updateDevice(Context&, Span<Vec2f> points, bool antialias);
	void draw(Context&, vk::CommandBuffer, bool stencil = true);

protected:
	vpp::BufferRange verts_ {};
	bool indirect_ {};
	bool antialias_ {};
	unsigned drawCount_ {};
};


class StrokePolygon {
public:
	void bake(Context&, Span<Vec2f> points, float width, LineCap, LineJoin),

	bool updateDevice(Context&);
	bool updateDeviceBake(Context&, Span<Vec2f> points,
		float width, LineCap, LineJoin, bool antilias);

protected:
	vpp::BufferRange verts_ {};
	bool indirect_ {};
	unsigned drawCount_ {};
};




class Polygon {
public:
	struct StrokeSettings {
		LineCap lineCap;
		LineJoin lineJoin;
		float width;
	};

	struct Baker {
		bool fill;
		std::optional<StrokeSettings> stroke;
	};

public:
	bool updateDevice(Context&, Span<Vec2f> points, const Baker&);

protected:
	struct {
		vpp::BufferRange buf_;
		size_t count_;
	} fill, stroke;
};


class DrawInstance {
public:
	void mask(const Polygon&);

	void stroke(const Paint&);
	void fill(const Paint&);

protected:
	vk::CommandBuffer cmdb_;
	std::vector<Polygon*> current_;
};






Shape someShape1;
Shape someShape2;

draw.mask(someShape1);
draw.mask(someShape2);
draw.stroke(somePaint);

draw.mask(someShape2);
draw.fill(somePaint);











class PathPolygon {
public:
	Path path;
	DrawMode draw;

public:
	void update();
	bool updateDevice(const Context&);
	bool updateDevice(const Context&, bool newIndirect);

	void fill(const DrawInstance&) const;
	void stroke(const DrawInstance&) const;

	const auto& polygon() const { return polygon_; }

protected:
	Polygon polygon_;
};


enum class LineCap {
	butt = 0,
	round,
	square,
};

enum class LineJoin {
	miter = 0,
	round,
	bevel
};








// gui layouts
auto& row = window.create<Row>();
row.create<Button>("label1");
row.create<Button>("label2");
row.create<Textfield>();

auto& nextRow = window.create<Row>();
// ...

// #2
auto& row = window.create<Row>();

{
	auto& layout = row.beginLayout();
	layout.create<Button>("label1");
	layout.create<Button>("label2");
	layout.create<Textfield>();
}

auto& nextRow = window.create<Row>();
// ...


class Window {
public:

protected:
	std::vector<std::unique_ptr<Widget>> widgets_;
};

class Row : public Widget {
public:
	Row(float height);

	template<typename W> W& createSized();
	template<typename W> W& create();

	void add(std::unique_ptr<Widget>);

protected:
	std::vector<std::unique_ptr<Widget>> widgets_;
};









/// Interface for an control on the screen that potentially handles input
/// events and can draw itself. Typical implementations of this interface
/// are Button, Textfield or ColorPicker.
class Widget : public nytl::NonMovable {
public:
	Widget(Gui& gui) : gui_(gui) {}
	virtual ~Widget() = default;

	/// Resizes this widget. Note that not all widgets are resizable equally.
	/// Some might throw when an invalid size is given or just display
	/// their content incorrectly.
	virtual void size(const Vec2f&) = 0;

	/// Returns whether the widget contains the given point.
	virtual bool contains(Vec2f point) const;

	/// Changes the widgets position.
	virtual void position(Vec2f);

	/// Instructs the widget to intersect its own scissor with the given rect.
	/// Note that this resets any previous scissor intersections.
	virtual void intersectScissor(const Rect2f&);

	/// Called when the Widget has registered itself for update.
	/// Gets the delta time since the last frame in seconds.
	/// Must not touch resources used for rendering.
	virtual void update(double) {}

	/// Called when the Widget has registered itsefl for updateDevice.
	/// Called when no rendering is currently done, so the widget might
	/// update rendering resources.
	virtual bool updateDevice() { return false; }

	/// Called during a drawing instance.
	/// The widget should draw itself.
	/// The default implementation handles the scissor and transform stuff
	/// automatically and excepts the derived class to implement the protected
	/// doDraw method.
	virtual void draw(const DrawInstance&) const;

	// - input processing -
	// all positions are given relative to the widget
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

protected:
	/// If the Widget does not override draw, this method will be called
	/// from it, with the transform and scissor correctly in place.
	virtual void doDraw(const DrawInstance&) const {}

	/// Registers this widgets for an update/updateDevice callback as soon
	/// as possible.
	void registerUpdate();
	void registerUpdateDevice();

protected:
	Gui& gui_;
	Rect2f bounds_;
	Transform transform_;
	Scissor scissor_;
};
