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

// fwd decls from nk_font
struct nk_font;
struct nk_font_atlas;

namespace vgv {

using namespace nytl;
class Context;

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
	void bind(Context&, vk::CommandBuffer);

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
};

struct Color { float r {}, g {}, b {}, a {}; };

/// A buffer holding paint data on the device.
/// Useful for more fine-grained control than Paint.
/// Used with PaintBinding to bind it for drawing.
class PaintBuffer {
public:
	/// The color used to draw.
	/// Can be freely changed, but changes will only be applied
	/// to the device when updateDevice is called.
	Color color;

public:
	PaintBuffer() = default;
	PaintBuffer(Context&, const Color& color);

	/// Uploads changes to the device.
	/// Will never invalidate/recreate any resources.
	/// Must not be called while this a CommandBuffer that uses
	/// this PaintBuffer in any way has not completed.
	void updateDevice();

protected:
	vpp::BufferRange ubo_;
};

/// Used to bind a PaintBuffer for drawing.
/// Useful for more fine-grained control than Paint.
class PaintBinding {
public:
	PaintBinding() = default;
	PaintBinding(Context&, const PaintBuffer& buffer);

	/// Binds the associated PaintBuffer for drawing in the given
	/// CommandBuffer.
	void bind(Context&, vk::CommandBuffer);

	/// Changes the associated PaintBuffer. Can be called at any
	/// time but will only be updated to the device when updateDevice
	/// is called. Returns the old buffer (if any).
	const PaintBuffer* buffer(const PaintBuffer& newbuf);

	/// Uploads a changed PaintBuffer to the device.
	/// Will never invalidate/recreate any resources.
	/// Must not be called while this a CommandBuffer that uses
	/// this PaintBinding in any way has not completed.
	void updateDevice();

protected:
	const PaintBuffer* buffer_;
	vpp::DescriptorSet ds_;
};

/// Defines how shapes are drawn.
/// For a more fine-grained control see PaintBinding and PaintBuffer.
class Paint {
public:
	Color color;

public:
	Paint() = default;
	Paint(Context& ctx, const Color& color);

	void bind(Context&, vk::CommandBuffer);
	void updateDevice();

protected:
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

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

enum class PolygonMode {
	fan = 0,
	list,
	strip,
};

class Polygon {
public:
	std::vector<Vec2f> points {};
	PolygonMode mode {};

public:
	Polygon() = default;
	Polygon(Context&, PolygonMode = PolygonMode::fan, bool indirect = false);

	bool updateDevice(Context&);
	void draw(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange verts_ {};
	bool indirect_ {};
};

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
	std::string text {};
	const Font* font {};
	Vec2f pos {};

public:
	Text() = default;
	Text(Context&, std::string text, const Font& font, Vec2f pos,
		bool indirect = false);

	bool updateDevice(Context&);
	void draw(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange positionBuf_;
	vpp::BufferRange uvBuf_;
	bool indirect_ {};
	unsigned drawCount_ {};
};

} // namespace vgv
