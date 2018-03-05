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
	Paint(Context& ctx, const Color& color);
	void bind(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange ubo_;
	vpp::DescriptorSet ds_;
};

class Context {
public:
	Context(vpp::Device& dev, vk::RenderPass, unsigned int subpass);

	const vpp::Device& device() const;
	vk::Pipeline fanPipe() const { return fanPipe_; }
	vk::PipelineLayout pipeLayout() const { return pipeLayout_; }
	const auto& dsLayout() const { return dsLayout_; }
	const auto& dsPool() const { return dsPool_; }

private:
	vpp::Pipeline fanPipe_;
	vpp::PipelineLayout pipeLayout_;
	vpp::DescriptorSetLayout dsLayout_;
	vpp::DescriptorPool dsPool_;
	// vpp::Sampler sampler_;
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

/*
class FontAtlas {
};

class Font {
	FontAtlas* atlas;
};

class Text {
public:
	Text(Context&, const char* text, Font& font);

	Font& font(Font& newFont);
	const char* text(const char* newText);

	bool updateDevice();
	void draw(Context&, vk::CommandBuffer);

protected:
	vpp::BufferRange buffer_;
	const char* text_;
	Font* font_;
};
*/

} // namespace vgv







