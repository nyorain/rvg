#pragma once

#include <vpp/fwd.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>
#include <vector>

namespace vgv {

class Context;

struct Color { float r, g, b, a; };
struct Vec2f { float x, y; };

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

class Context {
public:
	Context(vpp::Device& dev, vk::RenderPass, unsigned int subpass);

	const vpp::Device& device() const { return device_; };
	vk::Pipeline fanPipe() const { return fanPipe_; }
	vk::PipelineLayout pipeLayout() const { return pipeLayout_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutTex() const { return dsLayoutTex_; }
	const auto& dsPool() const { return dsPool_; }

	const auto& dummyTex() const { return dummyTex_; };

private:
	const vpp::Device& device_;
	vpp::Pipeline fanPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayoutPaint_;
	vpp::DescriptorSetLayout dsLayoutTex_;
	vpp::DescriptorPool dsPool_;
	vpp::Sampler sampler_;

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;
};

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

class Rect {
public:
	Rect(Context&);
	~Rect() = default;

	Vec2f position(Vec2f&);
	Vec2f size(Vec2f&);

	Vec2f position() const;
	Vec2f size() const;

	bool updateDevice(DrawMode);
	void stroke(Context&, vk::CommandBuffer);
	void fill(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange fill_ {};
	vpp::BufferRange stroke_ {};
	Vec2f pos_ {};
	Vec2f size_ {};
};

class FontAtlas {
protected:
	vpp::ViewableImage viewImg_;
};

struct Font {
};

class Text {
public:
	Font* font;
	const char* text;

public:
	Text(Context&, const char* text, Font& font);

	bool updateDevice();
	void draw(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange buffer_;
};

} // namespace vgv
