#pragma once

#include <vpp/fwd.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>

#include <nytl/vec.hpp>
#include <nytl/mat.hpp>
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
	vk::Pipeline listPipe() const { return listPipe_; }
	vk::Pipeline stripPipe() const { return stripPipe_; }

	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutTex() const { return dsLayoutTex_; }
	const auto& dsLayoutTransform() const { return dsLayoutTransform_; }

	const auto& dummyTex() const { return dummyTex_; };

private:
	const vpp::Device& device_;
	vpp::Pipeline fanPipe_;
	vpp::Pipeline listPipe_;
	vpp::Pipeline stripPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayoutPaint_;
	vpp::DescriptorSetLayout dsLayoutTex_;
	vpp::DescriptorSetLayout dsLayoutTransform_;
	vpp::DescriptorPool dsPool_;
	vpp::Sampler sampler_;

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;
	Transform identityTransform_;
};

struct Color { float r {}, g {}, b {}, a {}; };

/// A buffer holding paint data on the device.
/// Useful for more fine-grained control than Paint.
/// Used with PaintBinding to bind it for drawing.
class PaintBuffer {
public:
	PaintBuffer() = default;
	PaintBuffer(const Context&, const Color& color);

	/// Uploads changes to the device.
	/// Will never invalidate/recreate any resources.
	/// Must not be called while this a CommandBuffer that uses
	/// this PaintBuffer in any way has not completed.
	void updateDevice(const Color& color) const;
	const auto& ubo() const { return ubo_; }

protected:
	vpp::BufferRange ubo_;
};

/// Used to bind a PaintBuffer for drawing.
/// Useful for more fine-grained control than Paint.
class PaintBinding {
public:
	PaintBinding() = default;
	PaintBinding(const Context&, const PaintBuffer& buffer);

	/// Binds the associated PaintBuffer for drawing in the given
	/// DrawInstance.
	void bind(const DrawInstance&) const;

	/// Updates a changed PaintBuffer binding to the device.
	/// Will never invalidate/recreate any resources.
	/// Must not be called while this a CommandBuffer that uses
	/// this PaintBinding in any way has not completed.
	void updateDevice(const PaintBuffer&) const;
	const auto& ds() const { return ds_; }

protected:
	vpp::DescriptorSet ds_;
};

/// Defines how shapes are drawn.
/// For a more fine-grained control see PaintBinding and PaintBuffer.
class Paint {
public:
	Color color;

public:
	Paint() = default;
	Paint(Context&, const Color& color = {});

	void bind(const DrawInstance&);
	void updateDevice();

	const auto& buffer() const { return buffer_; }
	const auto& binding() const { return binding_; }

protected:
	PaintBuffer buffer_;
	PaintBinding binding_;
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
};

/// A shape defined by points that can be stroked or filled.
class Polygon {
public:
	Polygon(bool indirect = false) : indirect_(indirect) {}

	/// Returns whether a rerecord is needed.
	/// Can be called at any time, computes the polygon from the given
	/// points and draw mode. The DrawMode specifies whether this polygon
	/// can be used for filling or stroking and their properties.
	bool update(Span<const Vec2f> points, const DrawMode&);

	/// Uploads data to the device. Must not be called while a command
	/// buffer drawing the Polygon is executing.
	/// Returns whether a command buffer rerecord is needed.
	bool updateDevice(const Context&);

	/// Like the other updateDevice overload but updates whether the
	/// polygon should be drawn indirect.
	bool updateDevice(const Context&, bool newIndirect);

	/// Records commands to fill this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without fill support.
	void fill(const DrawInstance&) const;

	/// Records commands to stroke this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without stroke support.
	void stroke(const DrawInstance&) const;

protected:
	bool indirect_;

	std::vector<Vec2f> fillCache_;
	vpp::BufferRange fillBuf_;

	std::vector<Vec2f> strokeCache_;
	vpp::BufferRange strokeBuf_;
};

/// Convex shape specifies by its outlining points.
/// Can be filled to stroked.
class Shape {
public:
	std::vector<Vec2f> points;
	DrawMode draw;

public:
	Shape() = default;
	Shape(std::vector<Vec2f>, const DrawMode&);

	bool update();
	bool updateDevice(const Context&);
	bool updateDevice(const Context&, bool newIndirect);

	void fill(const DrawInstance&) const;
	void stroke(const DrawInstance&) const;

	const auto& polygon() const { return polygon_; }

protected:
	Polygon polygon_;
};

/// Rectangle shape that can be filled to stroked.
class RectShape {
public:
	Vec2f pos;
	Vec2f size;
	DrawMode draw;

public:
	RectShape() = default;
	RectShape(Vec2f p, Vec2f s, const DrawMode& mode);

	bool update();
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
	Vec2f pos {};

public:
	Text() = default;
	Text(std::u32string text, const Font&, Vec2f pos, bool indirect = false);
	Text(std::string text, const Font&, Vec2f pos, bool indirect = false);

	bool update();
	bool updateDevice(Context&);
	bool updateDevice(Context&, bool newIndirect);
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

protected:
	std::vector<Vec2f> posCache_;
	std::vector<Vec2f> uvCache_;
	vpp::BufferRange buf_;
	bool indirect_ {};
};

} // namespace vgv
