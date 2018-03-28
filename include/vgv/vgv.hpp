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

// fwd decls from nk_font
struct nk_font;
struct nk_font_atlas;

namespace vgv {

using namespace nytl;
class Context;

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
class Paint {
public:
	PaintData paint;

public:
	Paint() = default;
	Paint(Context&, const PaintData& data);

	void bind(const DrawInstance&) const;
	bool updateDevice(const Context&);

	const auto& ubo() const { return ubo_; }
	const auto& ds() const { return ds_; }

protected:
	vk::ImageView oldView_;
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

/// Matrix-based transform used to specify how shape coordinates
/// are mapped to the output.
class Transform {
public:
	Mat4f matrix;

public:
	Transform() = default;
	Transform(Context& ctx);
	Transform(Context& ctx, const Mat4f&);

	auto& ubo() const { return ubo_; }
	void updateDevice();
	void bind(const DrawInstance&);

protected:
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes.
class Context {
public:
	Context(vpp::Device& dev, vk::RenderPass, unsigned int subpass);

	const vpp::Device& device() const { return device_; };
	vk::PipelineLayout pipeLayout() const { return pipeLayout_; }
	const auto& dsPool() const { return dsPool_; }
	DrawInstance record(vk::CommandBuffer);

	vk::Pipeline fanPipe() const { return fanPipe_; }
	vk::Pipeline stripPipe() const { return stripPipe_; }

	const auto& dsLayoutTransform() const { return dsLayoutTransform_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutFontAtlas() const { return dsLayoutFontAtlas_; }

	const auto& emptyImage() const { return emptyImage_; };
	const auto& dummyTex() const { return dummyTex_; };

	const auto& pointColorPaint() const { return pointColorPaint_; }

private:
	const vpp::Device& device_;
	vpp::Pipeline fanPipe_;
	vpp::Pipeline stripPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayoutTransform_;
	vpp::DescriptorSetLayout dsLayoutPaint_;
	vpp::DescriptorSetLayout dsLayoutFontAtlas_;
	vpp::DescriptorPool dsPool_;

	vpp::Sampler fontSampler_;
	vpp::Sampler texSampler_;

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;
	Transform identityTransform_;
	Paint pointColorPaint_;
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
	bool fill {};
	float stroke {};

	struct {
		std::vector<Vec4u8> points {};
		bool fill {};
		bool stroke {};
	} color {};
};

/// A shape defined by points that can be stroked or filled.
class Polygon {
public:
	/// Returns whether a rerecord is needed.
	/// Can be called at any time, computes the polygon from the given
	/// points and draw mode. The DrawMode specifies whether this polygon
	/// can be used for filling or stroking and their properties.
	void update(Span<const Vec2f> points, const DrawMode&);

	/// Uploads data to the device. Must not be called while a command
	/// buffer drawing the Polygon is executing.
	/// Returns whether a command buffer rerecord is needed.
	bool updateDevice(const Context&, bool disable = false);

	/// Records commands to fill this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without fill support.
	void fill(const DrawInstance&) const;

	/// Records commands to stroke this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without stroke support.
	void stroke(const DrawInstance&) const;

protected:
	enum class State {
		fillColor = 1u,
		strokeColor = 2u,
	};

	nytl::Flags<State> state_ {};

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
	std::vector<Vec2f> points;
	DrawMode draw;
	bool hide {false};

public:
	Shape() = default;
	Shape(const Context&, std::vector<Vec2f> points, const DrawMode&,
		bool hide = false);

	void update();
	bool updateDevice(const Context&);
	void fill(const DrawInstance&) const;
	void stroke(const DrawInstance&) const;

	const auto& polygon() const { return polygon_; }

protected:
	Polygon polygon_;
};

/// Rectangle shape that can be filled to stroked.
class RectShape {
public:
	Vec2f position;
	Vec2f size;
	DrawMode draw;
	bool hide {false};

public:
	RectShape() = default;
	RectShape(const Context&, Vec2f pos, Vec2f size, const DrawMode&,
		bool hide = false);

	void update();
	bool updateDevice(const Context&);

	void fill(const DrawInstance&) const;
	void stroke(const DrawInstance&) const;

	const auto& polygon() const { return polygon_; }

protected:
	Polygon polygon_;
};

/// Circular shape that can be filled or stroked.
class CircleShape {
public:
	Vec2f center;
	Vec2f radius;
	DrawMode draw;
	bool hide {false};
	unsigned points {16};
	float startAngle {0.f};

public:
	CircleShape() = default;
	CircleShape(const Context&, Vec2f center, Vec2f radius, const DrawMode&,
		bool hide = false, unsigned points = 16, float startAngle = 0.f);
	CircleShape(const Context&, Vec2f center, float radius, const DrawMode&,
		bool hide = false, unsigned points = 16, float startAngle = 0.f);

	void update();
	bool updateDevice(const Context&);

	void fill(const DrawInstance&) const;
	void stroke(const DrawInstance&) const;

	const auto& polygon() const { return polygon_; }

protected:
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

	float width(StringParam text) const;
	float height() const;

	auto* nkFont() const { return font_; }
	auto& atlas() const { return *atlas_; }

protected:
	FontAtlas* atlas_;
	struct nk_font* font_;
};

/// Represents text to be drawn.
class Text {
public:
	std::u32string text {};
	const Font* font {};
	Vec2f position {};

public:
	Text() = default;
	Text(const Context&, std::u32string text, const Font&, Vec2f pos);
	Text(const Context&, std::string text, const Font&, Vec2f pos);

	void update();
	bool updateDevice(const Context&);
	void draw(const DrawInstance&) const;

	/// Computes which char index lies at the given relative x.
	/// For this to work, update() has to have been called.
	/// Returns the index + 1 of the last char before the given x value,
	/// and how far it lies into that char (from 0.f to 1.f).
	/// If the x value lies after the char in its following space, this
	/// is set to -1.f.
	/// Also returns the start of the nearest space char boundary.
	/// Example: Given there are two chars, the first from x=1 to x=11 and
	/// the second from x=15 to x=35:
	///  - charAt(0) returns {0, -1.f, 1.f}
	///  - charAt(1) returns {1, 0.f, 1.f}
	///  - charAt(6) returns {1, 0.5f, 1.f}
	///  - charAt(7) returns {1, 0.6f, 11.f}
	///  - charAt(13) returns {1, -1.f, 11.f}
	///  - charAt(30) returns {2, 2 / 3.f, 35.f}
	///  - charAt(36) returns {2, -1.f, 35.f}
	struct CharAt {
		unsigned last; // index of the last char before x
		float inside; // how far it lies into the given char, or -1.f if not
		float nearestBoundary; // next char boundary
	};

	CharAt charAt(float x) const;

	/// Returns the bounds of the ith char in local coordinates.
	Rect2f ithBounds(unsigned n) const;

protected:
	std::vector<Vec2f> posCache_;
	std::vector<Vec2f> uvCache_;
	vpp::BufferRange buf_;
};

constexpr auto transformBindSet = 0u;
constexpr auto paintBindSet = 1u;
constexpr auto fontBindSet = 2u;

} // namespace vgv
