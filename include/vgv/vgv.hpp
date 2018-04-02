#pragma once

#include <vpp/fwd.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>

#include <nytl/vec.hpp>
#include <nytl/mat.hpp>
#include <nytl/rect.hpp>
#include <nytl/stringParam.hpp>

#include <vector>
#include <string>
#include <variant>
#include <unordered_set>

// fwd decls from nk_font
struct nk_font;
struct nk_font_atlas;

namespace vgv {

using namespace nytl;
class Context;
class Polygon;
class Transform;
class Scissor;
class Text;

/// Small RAII wrapper around changing a DeviceObjects contents.
/// As long as the object is alived, the state can be changed. When
/// it gets destructed, the update function of the original object will
/// be called, i.e. the changed state will be applied.
template<typename T, typename S>
class StateChange {
public:
	T& object;
	S& state;

public:
	// TODO: catch all exception from object.update()?
	// might be problematic otherwise since this might throw from
	// destructor. Or only catch exceptions if destructor is called
	// due to unrolling (nytl/scope like)?
	// TODO: somehow assure that there is only ever one StateChange for one
	// object at a time?
	~StateChange() { object.update(); }

	S* operator->() { return &state; }
	S& operator*() { return state; }
};

template<typename T, typename S>
StateChange(T&, S&) -> StateChange<T, S>;

/// Simple base class for objects that register themself for
/// updateDevice callbacks at their associated Context.
class DeviceObject {
public:
	DeviceObject() = default;
	DeviceObject(Context& ctx) : context_(&ctx) {}
	~DeviceObject();

	DeviceObject(DeviceObject&& rhs) noexcept;
	DeviceObject& operator=(DeviceObject&& rhs) noexcept;

	Context& context() const { return *context_; }
	bool valid() const { return context_; }
	void update();

private:
	Context* context_ {};
};

/// Represents a render pass instance using a context.
struct DrawInstance {
	Context& context;
	vk::CommandBuffer cmdBuf;
};

// - Color -
using u8 = std::uint8_t;
constexpr struct Norm {} norm;

/// RGBA32 color tuple.
/// Offers various utility and conversion functions below.
class Color {
public:
	u8 r {0};
	u8 g {0};
	u8 b {0};
	u8 a {255};

public:
	Color() = default;

	Color(u8 r, u8 g, u8 b, u8 a = 255);
	Color(Vec3u8 rgb, u8 a = 255);
	Color(Vec4u8 rgba);

	Color(Norm, float r, float g, float b, float a = 1.f);
	Color(Norm, Vec3f rgb, float a = 1.f);
	Color(Norm, Vec4f rgba);

	Vec3u8 rgb() const { return {r, g, b}; }
	Vec4u8 rgba() const { return {r, g, b, a}; }

	Vec3f rgbNorm() const;
	Vec4f rgbaNorm() const;

public:
	const static Color white;
	const static Color black;
	const static Color red;
	const static Color green;
	const static Color blue;
};

// hsl
Color hsl(u8 h, u8 s, u8 l, u8 a = 255);
Color hslNorm(float h, float s, float l, float a = 1.f);

Vec3u8 hsl(const Color&);
Vec4u8 hsla(const Color&);

Vec3f hslNorm(const Color&);
Vec4f hslaNorm(const Color&);

// hsv
Color hsv(u8 h, u8 s, u8 v, u8 a = 255);
Color hsvNorm(float h, float s, float v, float a = 1.f);

Vec3u8 hsv(const Color&);
Vec4u8 hsva(const Color&);

Vec3f hsvNorm(const Color&);
Vec4f hsvaNorm(const Color&);

// hsv/hsl conversion utility
Vec3f hsl2hsv(Vec3f hsl);
Vec3f hsv2hsl(Vec3f hsv);

/// De/En-codes color as 32 bit rgba packed values.
/// This means the most significant byte will always hold the red value, the
/// actual byte order depends on the endianess of the machine.
Color u32rgba(std::uint32_t);
std::uint32_t u32rgba(const Color&);

/// Returns (fac * a + (1 - fac) * b), i.e. mixes the given colors
/// with the given factor (which should be in range [0,1]).
Color mix(const Color& a, const Color& b, float fac);


// - Paint -
enum class PaintType : std::uint32_t {
	color = 1,
	linGrad = 2,
	radGrad = 3,
	textureRGBA = 4,
	textureA = 5,
	pointColor = 6,
};

struct FragPaintData {
	Color inner;
	Color outer;
	Vec4f custom;
	PaintType type {};
};

struct DevicePaintData {
	nytl::Mat4f transform {};
	FragPaintData frag;
};

/// Host representation of a paint.
struct PaintData {
	vk::ImageView texture {};
	DevicePaintData data;
};

PaintData colorPaint(const Color&);
PaintData linearGradient(Vec2f start, Vec2f end,
	const Color& startColor, const Color& endColor);
PaintData radialGradient(Vec2f center, float innerRadius, float outerRadius,
	const Color& innerColor, const Color& outerColor);
PaintData texturePaintRGBA(const nytl::Mat4f& transform, vk::ImageView);
PaintData texturePaintA(const nytl::Mat4f& transform, vk::ImageView);
PaintData pointColorPaint();

/// Defines how shapes are drawn.
/// For a more fine-grained control see PaintBinding and PaintBuffer.
class Paint : public DeviceObject {
public:
	Paint() = default;
	Paint(Context&, const PaintData& data);

	/// Binds the Paint object in the given DrawInstance.
	/// Following calls to fill/stroke of a polygon-based shape
	/// or Text::draw will use this bound paint until another
	/// Paint is bound.
	void bind(const DrawInstance&) const;

	auto change() { return StateChange {*this, paint_}; }
	void paint(const PaintData& data) { *change() = data; }
	const auto& paint() const { return paint_; }

	const auto& ubo() const { return ubo_; }
	const auto& ds() const { return ds_; }

	void update();
	bool updateDevice();

protected:
	PaintData paint_ {};
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
	vk::ImageView oldView_ {};
};

/// Matrix-based transform used to specify how shape coordinates
/// are mapped to the output.
class Transform : public DeviceObject {
public:
	Transform() = default;
	Transform(Context& ctx); // uses identity matrix
	Transform(Context& ctx, const Mat4f&);

	/// Binds the transform in the given DrawInstance.
	/// All following stroke/fill calls are affected by this transform object,
	/// until another transform is bound.
	void bind(const DrawInstance&) const;

	auto change() { return StateChange {*this, matrix_}; }
	auto& matrix() const { return matrix_; }
	void matrix(const Mat4f& matrix) { *change() = matrix; }

	auto& ubo() const { return ubo_; }
	auto& ds() const { return ds_; }

	void update();
	bool updateDevice();

protected:
	Mat4f matrix_;
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

/// Limits the area in which can be drawn.
/// Scissor is applied before the transformation.
class Scissor : public DeviceObject {
public:
	static constexpr Rect2f reset = {-1e6, -1e6, 2e6, 2e6};

public:
	Scissor() = default;
	Scissor(Context&, const Rect2f& = reset);

	/// Binds the scissor in the given DrawInstance.
	/// All following stroke/fill calls are affected by this scissor object,
	/// until another scissor is bound.
	void bind(const DrawInstance&) const;

	auto change() { return StateChange {*this, rect_}; }
	void rect(const Rect2f& rect) { *change() = rect; }
	auto& rect() const { return rect_; }

	auto& ubo() const { return ubo_; }
	auto& ds() const { return ds_; }
	void update();
	bool updateDevice();

protected:
	Rect2f rect_ = reset;
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

struct ContextSettings {
	/// The renderpass and subpass in which it will be used.
	/// Must be specified for pipeline creation.
	vk::RenderPass renderPass;
	unsigned subpass {0};

	/// Whether the device has the clipDistance feature enabled.
	/// Can increase scissor performance significantly.
	bool clipDistanceEnable {false};
};

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes.
class Context {
public:
	using DeviceObject = std::variant<
		Polygon*,
		Text*,
		Paint*,
		Transform*,
		Scissor*>;

public:
	Context(vpp::Device&, const ContextSettings&);

	bool updateDevice();
	DrawInstance record(vk::CommandBuffer);

	void rerecord() { rerecord_ = true; }

	const vpp::Device& device() const { return device_; };
	vk::PipelineLayout pipeLayout() const { return pipeLayout_; }
	const auto& dsPool() const { return dsPool_; }

	vk::Pipeline fanPipe() const { return fanPipe_; }
	vk::Pipeline stripPipe() const { return stripPipe_; }

	const auto& dsLayoutTransform() const { return dsLayoutTransform_; }
	const auto& dsLayoutScissor() const { return dsLayoutScissor_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutFontAtlas() const { return dsLayoutFontAtlas_; }

	const auto& emptyImage() const { return emptyImage_; };

	const auto& dummyTex() const { return dummyTex_; };
	const auto& pointColorPaint() const { return pointColorPaint_; }
	const auto& identityTransform() const { return identityTransform_; }
	const auto& defaultScissor() const { return defaultScissor_; }

	void registerUpdateDevice(DeviceObject);
	bool deviceObjectDestroyed(::vgv::DeviceObject&) noexcept;
	void deviceObjectMoved(::vgv::DeviceObject&, ::vgv::DeviceObject&) noexcept;

private:
	const vpp::Device& device_;
	vpp::Pipeline fanPipe_;
	vpp::Pipeline stripPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayoutTransform_;
	vpp::DescriptorSetLayout dsLayoutScissor_;
	vpp::DescriptorSetLayout dsLayoutPaint_;
	vpp::DescriptorSetLayout dsLayoutFontAtlas_;
	vpp::DescriptorPool dsPool_;

	vpp::Sampler fontSampler_;
	vpp::Sampler texSampler_;

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;

	Scissor defaultScissor_;
	Transform identityTransform_;
	Paint pointColorPaint_;

	std::unordered_set<DeviceObject> updateDevice_;
	bool rerecord_;
};

/// Type (format) of a texture.
enum class TextureType {
	rgba32,
	a8
};

/// Creates a vulkan texture that loads its contents from the given
/// file. Stores the viewable image in the returned image variable.
/// The image be allocated on device local memory.
/// It will start with general image layout.
/// Throws std::runtime_error if something goes wrong.
vpp::ViewableImage createTexture(const vpp::Device&,
	const char* filename, TextureType);
vpp::ViewableImage createTexture(const vpp::Device&, unsigned int width,
	unsigned int height, const std::byte* data, TextureType);

/// Specifies in which way a polygon can be drawn.
struct DrawMode {
	bool fill {}; /// Whether it can be filled
	float stroke {}; /// Whether it can be stroked

	/// Defines per-point color values.
	/// If the polygon is then filled/stroked with a pointColorPaint,
	/// will use those points.
	struct {
		std::vector<Vec4u8> points {}; /// the per-point color values
		bool fill {}; /// whether they can be used when filling
		bool stroke {}; /// whether they can be used when stroking
	} color {};
};

enum class DrawType {
	stroke,
	fill,
	strokeFill
};

/// A shape defined by points that can be stroked or filled.
class Polygon : public DeviceObject {
public:
	Polygon() = default;
	Polygon(Context&);

	/// Can be called at any time, computes the polygon from the given
	/// points and draw mode. The DrawMode specifies whether this polygon
	/// can be used for filling or stroking and their properties.
	/// Automatically registers this object for the next updateDevice call.
	void update(Span<const Vec2f> points, const DrawMode&);

	/// Changes the disable state of this polygon.
	/// Cheap way to hide/unhide the polygon, can be called at any
	/// time and will never trigger a rerecord.
	/// Automatically registers this object for the next updateDevice call.
	void disable(bool, DrawType = DrawType::strokeFill);
	bool disabled(DrawType = DrawType::strokeFill) const;

	/// Records commands to fill this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without fill support.
	void fill(const DrawInstance&) const;

	/// Records commands to stroke this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without stroke support.
	void stroke(const DrawInstance&) const;

	/// Usually only automatically called from context.
	/// Uploads data to the device. Must not be called while a command
	/// buffer drawing the Polygon is executing.
	/// Returns whether a command buffer rerecord is needed.
	bool updateDevice();

protected:
	struct {
		bool fillColor : 1;
		bool strokeColor : 1;
		bool disableFill : 1;
		bool disableStroke : 1;
	} flags_ {};

	std::vector<Vec2f> fillCache_;
	std::vector<Vec4u8> fillColorCache_;
	vpp::BufferRange fillBuf_;

	std::vector<Vec2f> strokeCache_;
	std::vector<Vec4u8> strokeColorCache_;
	vpp::BufferRange strokeBuf_;
};

/// Convex shape specifies by its outlining points.
/// Can be filled or stroked.
class Shape {
public:
	Shape() = default;
	Shape(Context&, std::vector<Vec2f> points, const DrawMode&);

	auto change() { return StateChange {*this, state_}; }

	void fill(const DrawInstance& di) const { return polygon_.fill(di); }
	void stroke(const DrawInstance& di) const { return polygon_.stroke(di); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& state() const { return state_; }
	const auto& polygon() const { return polygon_; }
	void update();

protected:
	struct State {
		std::vector<Vec2f> points;
		DrawMode draw;
	} state_;

	Polygon polygon_;
};

/// Rectangle shape that can be filled or stroked.
/// Can also have rounded corners.
class RectShape {
public:
	RectShape() = default;
	RectShape(Context&, Vec2f pos, Vec2f size, const DrawMode&,
		std::array<float, 4> round = {});

	auto change() { return StateChange {*this, state_}; }

	void fill(const DrawInstance& di) const { return polygon_.fill(di); }
	void stroke(const DrawInstance& di) const { return polygon_.stroke(di); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& state() const { return state_; }
	const auto& polygon() const { return polygon_; }
	void update();

protected:
	struct {
		Vec2f position;
		Vec2f size;
		DrawMode draw;
		std::array<float, 4> rounding {};
	} state_;

	Polygon polygon_;
};

/// Circular shape that can be filled or stroked.
class CircleShape {
public:
	CircleShape() = default;
	CircleShape(Context&, Vec2f center, Vec2f radius, const DrawMode&,
		unsigned points = 16, float startAngle = 0.f);
	CircleShape(Context&, Vec2f center, float radius, const DrawMode&,
		unsigned points = 16, float startAngle = 0.f);

	auto change() { return StateChange {*this, state_}; }

	void fill(const DrawInstance& di) const { return polygon_.fill(di); }
	void stroke(const DrawInstance& di) const { return polygon_.stroke(di); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& state() const { return state_; }
	const auto& polygon() const { return polygon_; }
	void update();

protected:
	struct {
		Vec2f center;
		Vec2f radius;
		DrawMode draw;
		unsigned points {16};
		float startAngle {0.f};
	} state_;

	Polygon polygon_;
};

/// Holds a texture on the device to which multiple fonts can be uploaded.
class FontAtlas {
public:
	FontAtlas(Context&);
	~FontAtlas();

	bool bake(Context&);

	auto& nkAtlas() const { return *atlas_; }
	auto& ds() const { return ds_; }
	auto& texture() const { return texture_; }

protected:
	std::unique_ptr<nk_font_atlas> atlas_;
	vpp::DescriptorSet ds_;
	vpp::ViewableImage texture_;
	unsigned width_ = 0, height_ = 0;
};

/// Represents information about one font in a font atlas.
class Font {
public:
	Font(FontAtlas&, const char* file, unsigned height);
	Font(FontAtlas&, struct nk_font* font);

	float width(std::string_view text) const;
	float width(std::u32string_view text) const;
	float height() const;

	auto* nkFont() const { return font_; }
	auto& atlas() const { return *atlas_; }

protected:
	FontAtlas* atlas_;
	struct nk_font* font_;
};

/// Represents text to be drawn.
class Text : public DeviceObject {
public:
	Text() = default;
	Text(Context&, std::u32string text, const Font&, Vec2f pos);
	Text(Context&, std::string_view text, const Font&, Vec2f pos);

	/// Draws this text with the bound draw resources (transform,
	/// scissor, paint).
	void draw(const DrawInstance&) const;

	auto change() { return StateChange {*this, state_}; }
	bool disable(bool);
	bool disabled() const { return disable_; }

	/// Computes which char index lies at the given relative x.
	/// Returns the index of the char at the given x, or the index of
	/// the next one if there isn't any. Returns text.length() if x is
	/// after all chars.
	unsigned charAt(float x) const;

	/// Returns the bounds of the ith char in local coordinates.
	/// For a char that has no x-size (e.g. space), returns xadvance
	/// as x size.
	Rect2f ithBounds(unsigned n) const;

	const auto& font() const { return state_.font; }
	const auto& utf32() const { return state_.utf32; }
	const auto& position() const { return state_.position; }
	auto utf8() const { return state_.utf8(); }
	float width() const;

	void update();
	bool updateDevice();

protected:
	struct State {
		std::u32string utf32 {};
		const Font* font {};
		Vec2f position {};

		void utf8(StringParam);
		std::string utf8() const;
	} state_;

	bool disable_ {};
	std::vector<Vec2f> posCache_;
	std::vector<Vec2f> uvCache_;
	vpp::BufferRange buf_;
	const Font* oldFont_ {};
};

// TODO: move as constants into Context
constexpr auto transformBindSet = 0u;
constexpr auto paintBindSet = 1u;
constexpr auto fontBindSet = 2u;
constexpr auto scissorBindSet = 3u;

} // namespace vgv
