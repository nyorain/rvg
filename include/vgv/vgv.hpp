#pragma once

#include <vpp/fwd.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>
#include <nytl/vec.hpp>
#include <vector>

// fwd decls from nk_font
struct nk_font;
struct nk_font_atlas;

namespace vgv {

using namespace nytl;

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes.
class Context {
public:
	Vec2f viewSize {0, 0};

public:
	Context(vpp::Device& dev, vk::RenderPass, unsigned int subpass);

	const vpp::Device& device() const { return device_; };
	vk::Pipeline fanPipe() const { return fanPipe_; }
	vk::Pipeline listPipe() const { return listPipe_; }
	vk::PipelineLayout pipeLayout() const { return pipeLayout_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutTex() const { return dsLayoutTex_; }
	const auto& dsPool() const { return dsPool_; }

	const auto& dummyTex() const { return dummyTex_; };

private:
	const vpp::Device& device_;
	vpp::Pipeline fanPipe_;
	vpp::Pipeline listPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayoutPaint_;
	vpp::DescriptorSetLayout dsLayoutTex_;
	vpp::DescriptorPool dsPool_;
	vpp::Sampler sampler_;

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;
};

struct Color { float r, g, b, a; };

class Paint {
public:
	Color color;

public:
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

enum class DrawMode {
	fill,
	stroke,
	both
};

class Polygon {
public:
	Polygon(Context&, bool indirect = false);
	~Polygon() = default;

	const auto& points() const { return points_; }
	auto& points() { return points_; }

	bool updateDevice(Context&, DrawMode mode);
	void stroke(Context&, vk::CommandBuffer);
	void fill(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange fill_ {};
	vpp::BufferRange stroke_ {};
	std::vector<Vec2f> points_ {};
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

	unsigned width(const char* text);
	auto* nkFont() const { return font_; }
	auto& atlas() const { return *atlas_; }

protected:
	FontAtlas* atlas_;
	struct nk_font* font_;
};

class Text {
public:
	const char* text;
	Font* font;
	Vec2f pos;

public:
	Text(Context&, const char* text, Font& font, Vec2f pos);

	bool updateDevice(Context&);
	void draw(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange verts_;
};

} // namespace vgv
