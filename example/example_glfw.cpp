// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

// Example utility for creating window and vulkan swapchain setup
#include "render.hpp"

// the used rvg headers.
#include <rvg/context.hpp>
#include <rvg/shapes.hpp>
#include <rvg/paint.hpp>
#include <rvg/state.hpp>
#include <rvg/font.hpp>
#include <rvg/text.hpp>

// katachi to build our bezier curves or use svg
#include <katachi/path.hpp>
#include <katachi/svg.hpp>

// vpp to allow more high-level vulkan usage.
#include <vpp/handles.hpp>
#include <vpp/debug.hpp>
#include <vpp/formats.hpp>
#include <vpp/physicalDevice.hpp>

// some matrix/vector utilities
#include <nytl/vecOps.hpp>
#include <nytl/matOps.hpp>

// logging/debugging
#include <dlg/dlg.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <chrono>
#include <array>

// where the resources are located.
static const std::string baseResPath = "../";

using namespace nytl;
using namespace nytl::vec::operators;
using namespace nytl::vec::cw::operators;

// settings
constexpr auto appName = "rvg-example";
constexpr auto engineName = "vpp;rvg";
constexpr auto useValidation = true;
constexpr auto startMsaa = vk::SampleCountBits::e1;
constexpr auto layerName = "VK_LAYER_KHRONOS_validation";
constexpr auto printFrames = true;
constexpr auto vsync = true;
constexpr auto clearColor = std::array<float, 4>{{0.f, 0.f, 0.f, 1.f}};
constexpr auto textHeight = 16;

struct Context {
	rvg::Context& ctx;
	rvg::Font& font;
};

/// Visualizes correctly and wrongly interpolated gradients.
/// It displays three horizontal gradients: the first manually
/// mixed correctly (by using rvg::mix), the second by using a rvg
/// linear gradient. The third is mixed manually but incorrectly, by
/// simply calculating 'fac * colorA + (1 - fac) * colorB' which will
/// result in a somewhat uglier (and obviously different and incorrect)
/// gradient (as is sadly currently the default in most software).
class GradientWidget {
public:
	GradientWidget() = default;
	GradientWidget(const Context&, Vec2f pos,
		rvg::Color start, rvg::Color end);
	void draw(vk::CommandBuffer cmdBuf);

protected:
	struct Rect {
		rvg::RectShape shape;
		rvg::Paint paint;
	};

	rvg::Text topText_;
	std::vector<Rect> topRects_;

	rvg::Text middleText_;
	rvg::RectShape middleShape_;
	rvg::Paint gradient_;

	rvg::Text bottomText_;
	std::vector<Rect> bottomRects_;
};

class PathWidget {
public:
	PathWidget() = default;
	PathWidget(const Context&, Vec2f pos, Vec2f size);
	void draw(vk::CommandBuffer);
	void clicked(Vec2f pos);

	Vec2f pos() const { return pos_; }
	Vec2f size() const { return size_; }
	auto& shape() { return shape_; }

protected:
	bool first_ {true};
	Vec2f pos_;
	Vec2f size_;

	ktc::Subpath path_;
	rvg::Scissor scissor_;
	rvg::Shape shape_;
	rvg::RectShape bg_;
	rvg::Paint paint_;
	rvg::Text text_;
};

class PendulumWidget {
public:
	struct {
		float j {0.05};
		float c {0.05};
		float l {0.5};
		float m {0.5};
		float g {-9.81}; // we work in window coordinates
	} constants;

	float screenLength {350.f};
	float angle {0.01f};
	float avel {0.f};

	// in last step
	float accel {0.f};

public:
	PendulumWidget() = default;
	PendulumWidget(const Context& cctx, nytl::Vec2f pos);
	void update(float dt);
	nytl::Vec2f massPos() const;
	void changeCenter(nytl::Vec2f nc);
	void draw(vk::CommandBuffer cb);

	void left(bool pressed);
	void right(bool pressed);

protected:
	bool left_ {}, right_ {};
	nytl::Vec2f center_;
	rvg::Text text_;
	rvg::CircleShape fixture_;
	rvg::CircleShape mass_;
	rvg::Shape connection_;

	rvg::Paint whitePaint_;
	rvg::Paint redPaint_;

	float xVel_ {};
	float xFriction_ {8.f}; // not as it should be...
};

class App {
public:
	App(rvg::Context& ctx);

	void update(double dt);
	void draw(vk::CommandBuffer);
	void resize(Vec2ui size);

	void clicked(Vec2f pos);
	void key(unsigned, bool pressed);

protected:
	rvg::Context& ctx_;
	rvg::Font font_;
	rvg::Font awesomeFont_;

	GradientWidget gradWidget_;
	PendulumWidget pendulum_;
	PathWidget path_;

	rvg::Paint paint_;
	rvg::Transform transform_;
	rvg::CircleShape circle_;
	rvg::Text bottomText_;
	rvg::Shape starShape_;
	rvg::Shape hsvCircle_;

	rvg::Texture texture_;
	rvg::Paint texturePaint_;
	rvg::Paint gradientPaint_;

	std::vector<rvg::Text> texts_;
	double angle_ {};
	nytl::Vec2f scale_ {1.f, 1.f};
	nytl::Vec2ui size_ {};
};

// - implementation -
template<typename T>
void scale(nytl::Mat4<T>& mat, nytl::Vec3<T> fac) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][i] *= fac[i];
	}
}

template<typename T>
void translate(nytl::Mat4<T>& mat, nytl::Vec3<T> move) {
	for(auto i = 0; i < 3; ++i) {
		mat[i][3] += move[i];
	}
}

template<typename T = float>
nytl::Mat4<T> rotMat(double angle) {
	auto mat = nytl::identity<4, T>();

	auto c = std::cos(angle);
	auto s = std::sin(angle);

	mat[0][0] = c;
	mat[0][1] = -s;
	mat[1][0] = s;
	mat[1][1] = c;

	return mat;
}

enum class HorzAlign {
	left,
	center,
	right
};

enum class VertAlign {
	top,
	middle,
	bottom
};

/// Returns the position to align the text in the given rect
Vec2f alignText(std::string_view text, const rvg::Font& font,
		float height, Rect2f rect, HorzAlign halign = HorzAlign::center,
		VertAlign valign = VertAlign::middle) {

	auto size = Vec2f{font.width(text, height), height};

	Vec2f ret = rect.position;
	if(halign == HorzAlign::center) {
		ret.x += (rect.size.x - size.x) / 2.f;
	} else if(halign == HorzAlign::right) {
		ret.x += rect.size.x - size.x;
	}

	if(valign == VertAlign::middle) {
		ret.y += (rect.size.y - size.y) / 2.f;
	} else if(valign == VertAlign::bottom) {
		ret.y += rect.size.y - size.y;
	}

	return ret;
}

// Pendulum
PendulumWidget::PendulumWidget(const Context& cctx, nytl::Vec2f pos)
		: center_(pos) {

	constexpr auto centerRadius = 10.f;
	constexpr auto massRadius = 20.f;

	auto& ctx = cctx.ctx;
	auto end = massPos();
	auto drawMode = rvg::DrawMode {};
	drawMode.aaFill = true;
	drawMode.fill = true;
	fixture_ = {ctx, pos, centerRadius, drawMode};
	mass_ = {ctx, end, massRadius, drawMode};

	drawMode.fill = false;
	drawMode.stroke = 3.f;
	drawMode.aaStroke = true;
	connection_ = {ctx, {pos, end}, drawMode};

	whitePaint_ = {ctx, rvg::colorPaint(rvg::Color::white)};
	redPaint_ = {ctx, rvg::colorPaint(rvg::Color::red)};

	auto ppos = pos - Vec2f {0.f, massRadius + 10.f};
	auto string = "Move me using the arrow keys";
	auto textPos = alignText(string, cctx.font, textHeight, {ppos, {}});
	text_ = {ctx, textPos, string, cctx.font, textHeight};
}

void PendulumWidget::update(float dt) {
	constexpr auto leftBound = 650.f;
	constexpr auto rightBound = 1450.f;

	float u = -xFriction_ * xVel_;
	if(center_.x < leftBound) {
		center_.x = leftBound;
		u = -xVel_ / dt;
	} else if(left_) {
		u -= 5.f;
	}

	if(center_.x > rightBound) {
		center_.x = rightBound;
		u = -xVel_ / dt;
	} else if(right_) {
		u += 5.f;
	}

	xVel_ += dt * u;

	auto scale = 400.f;
	center_.x += scale * dt * xVel_;
	changeCenter(center_);

	// logic
	angle += dt * avel;

	auto& c = constants;
	accel = c.m * c.l * (u * std::cos(angle) + c.g * std::sin(angle));
	accel -= c.c * avel;
	accel /= (c.j + c.m * c.l * c.l);
	avel += dt * accel;

	// rendering
	auto end = massPos();
	mass_.change()->center = end;
	connection_.change()->points[1] = end;
}

nytl::Vec2f PendulumWidget::massPos() const {
	float c = std::cos(angle + 0.5 * nytl::constants::pi);
	float s = std::sin(angle + 0.5 * nytl::constants::pi);
	return center_ + screenLength * nytl::Vec2f{c, s};
}

void PendulumWidget::changeCenter(nytl::Vec2f nc) {
	center_ = nc;
	auto end = massPos();
	mass_.change()->center = end;
	connection_.change()->points = {nc, end};
	fixture_.change()->center = nc;
}

void PendulumWidget::left(bool pressed) {
	left_ = pressed;
}

void PendulumWidget::right(bool pressed) {
	right_ = pressed;
}

void PendulumWidget::draw(vk::CommandBuffer cb) {
	whitePaint_.bind(cb);
	fixture_.fill(cb);
	connection_.stroke(cb);
	text_.draw(cb);

	redPaint_.bind(cb);
	mass_.fill(cb);
}

// GradientWidget
GradientWidget::GradientWidget(const Context& ctx, Vec2f pos,
		rvg::Color start, rvg::Color end) {

	constexpr auto lineHeight = 100;
	constexpr auto textWidth = 250;
	constexpr auto gradientWidth = 300;
	constexpr auto gradientSteps = 32;
	constexpr auto stepWidth = float(gradientWidth) / gradientSteps;

	auto& rctx = ctx.ctx;
	auto dm = rvg::DrawMode {true, false};

	auto yoff = (lineHeight - textHeight) / 2;
	auto p = pos + Vec{10, yoff};
	topText_ = {rctx, p, "[Stepped] Using rvg::mix:", ctx.font, textHeight};

	p.y += lineHeight;
	middleText_ = {rctx, p, "Using a linear gradient:", ctx.font, textHeight};
	auto mpos = pos + Vec {textWidth, lineHeight};
	auto msize = Vec2f {gradientWidth, lineHeight};
	middleShape_ = {rctx, mpos, msize, dm};
	gradient_ = {rctx, rvg::linearGradient(mpos,
		mpos + Vec {gradientWidth, 0.f}, start, end)};

	p.y += lineHeight;
	bottomText_ = {rctx, p, "[Stepped] Using incorrect mixing", ctx.font,
		textHeight};

	auto rsize = Vec {stepWidth, lineHeight};
	auto topy = pos.y;
	auto boty = pos.y + 2 * lineHeight;
	for(auto i = 0u; i < gradientSteps; ++i) {
		auto fac = float(i) / (gradientSteps - 1);
		auto x = pos.x + textWidth + i * stepWidth;

		topRects_.emplace_back();
		auto p1 = rvg::colorPaint(rvg::mix(start, end, 1 - fac));
		topRects_.back().paint = {rctx, p1};
		topRects_.back().shape = {rctx, {x, topy}, rsize, dm};

		bottomRects_.emplace_back();
		auto col = (1 - fac) * start.rgbaNorm() + fac * end.rgbaNorm();
		auto p2 = rvg::colorPaint({rvg::norm, col});
		bottomRects_.back().paint = {rctx, p2};
		bottomRects_.back().shape = {rctx, {x, boty}, rsize, dm};
	}
}

void GradientWidget::draw(vk::CommandBuffer cb) {
	topText_.draw(cb);
	middleText_.draw(cb);
	bottomText_.draw(cb);

	dlg_assert(topRects_.size() == bottomRects_.size());
	for(auto i = 0u; i < topRects_.size(); ++i) {
		topRects_[i].paint.bind(cb);
		topRects_[i].shape.fill(cb);

		bottomRects_[i].paint.bind(cb);
		bottomRects_[i].shape.fill(cb);
	}

	gradient_.bind(cb);
	middleShape_.fill(cb);
}

// Path
PathWidget::PathWidget(const Context& ctx, Vec2f pos, Vec2f size) {
	pos_ = pos;
	size_ = size;

	auto drawMode = rvg::DrawMode {false, 1.f};
	drawMode.aaStroke = true;
	drawMode.deviceLocal = true;
	shape_ = {ctx.ctx, {}, drawMode};
	scissor_ = {ctx.ctx, {pos, size}};

	drawMode.stroke = 4.f;
	bg_ = {ctx.ctx, pos + Vec {2.f, 2.f}, size - Vec {4.f, 4.f},
		drawMode, {4.f, 4.f, 4.f, 4.f}};
	paint_ = {ctx.ctx, rvg::colorPaint(rvg::Color::white)};
}

void PathWidget::draw(vk::CommandBuffer cb) {
	scissor_.bind(cb);
	paint_.bind(cb);
	bg_.stroke(cb);
	shape_.stroke(cb);
	scissor_.context().defaultScissor().bind(cb);
}

void PathWidget::clicked(Vec2f pos) {
	if(first_) {
		first_ = false;
		path_.start = pos;
	} else {
		path_.sqBezier(pos);
		shape_.change()->points = ktc::flatten(path_);
	}
}

// App
App::App(rvg::Context& ctx) : ctx_(ctx),
		font_(ctx_, baseResPath + "example/OpenSans-Regular.ttf"),
		awesomeFont_(ctx_, baseResPath + "example/fontawesome-webfont.ttf") {

	constexpr auto gradPos = Vec {50.f, 50.f};
	constexpr auto pathPos = Vec {50.f, 450.f};
	constexpr auto pathSize = Vec {400.f, 400.f};
	constexpr auto pendulumPos = Vec {900.f, 450.f};
	constexpr auto texPath = "example/thunderstorm.jpg";
	constexpr auto circlePos = Vec {850.f, 200.f};
	constexpr auto circleRad = 120.f;
	constexpr auto hsvCenter = Vec {1150.f, 200.f};
	constexpr auto hsvRad = 120.f;
	constexpr auto starPos = Vec {1440.f, 205.f};
	constexpr auto starRad = 130.f;
	constexpr auto textOff = Vec {0.f, 150.f};

	font_.fallback(awesomeFont_);

	auto addText = [&](Vec2f center, const char* string) {
		auto pos = alignText(string, font_, textHeight, {center, {}});
		texts_.emplace_back(ctx, pos, string, font_, textHeight);
	};

	transform_ = {ctx};
	gradWidget_ = {{ctx, font_}, gradPos, rvg::Color::red, rvg::Color::green};
	path_ = {{ctx, font_}, pathPos, pathSize};
	pendulum_ = {{ctx, font_}, pendulumPos};
	bottomText_ = {ctx, {}, u8" https://github.com/nyorain/rvg ", font_,
		textHeight};
	paint_ = {ctx, rvg::colorPaint(rvg::Color::white)};

	addText(pathPos + Vec {pathSize.x / 2.f, pathSize.y + 20.f},
		"Click me: Anti aliased smooth bezier curves");

	texture_ = rvg::Texture(ctx, baseResPath + texPath);

	auto mat = nytl::identity<4, float>();
	mat[0][0] = 0.5 / circleRad;
	mat[1][1] = 0.5 / circleRad;
	mat[0][3] = -0.5 * (circlePos.x - circleRad) / circleRad;
	mat[1][3] = -0.5 * (circlePos.y - circleRad) / circleRad;
	texturePaint_ = {ctx, rvg::texturePaintRGBA(mat, texture_.vkImageView())};

	// star
	auto starPoints = [&](Vec2f center, float scale) {
		std::vector<nytl::Vec2f> points;
		points.push_back(center);
		using nytl::constants::pi;
		auto angle = -0.5 * pi;
		for(auto i = 0u; i < 6; ++i) {
			auto pos = center + scale * Vec {std::cos(angle), std::sin(angle)};
			points.push_back(Vec2f(pos));
			angle += (4 / 5.f) * pi;
		}

		return points;
	};

	starShape_ = {ctx, starPoints(starPos, starRad), {true, 2.f}};
	gradientPaint_ = {ctx, rvg::radialGradient(starPos, -5.f, starRad - 15.f,
		rvg::u32rgba(0x7474FFFF), rvg::u32rgba(0xFFFF74FF))};
	addText(starPos + textOff, "No anti aliasing");

	// hsv wheel
	auto drawMode = rvg::DrawMode {true, 0.f};

	auto hsvPointCount = 128u + 2u;
	auto colorPoints = std::vector<nytl::Vec4u8> {hsvPointCount};
	auto hsvPoints = std::vector<nytl::Vec2f> {hsvPointCount};
	colorPoints[0] = rvg::hsvNorm(0.f, 0.f, 0.5f).rgba();
	hsvPoints[0] = hsvCenter;
	for(auto i = 1u; i < hsvPointCount; ++i) {
		auto fac = (i - 1) / float(hsvPointCount - 2);
		auto col = rvg::hsvNorm(fac, 1.f, 1.f);
		colorPoints[i] = col.rgba();
		fac *= 2 * nytl::constants::pi;
		auto off = Vec2f {std::cos(fac), std::sin(fac)};
		hsvPoints[i] = hsvCenter + hsvRad * off;
	}

	drawMode.color.fill = true;
	drawMode.color.points = std::move(colorPoints);
	hsvCircle_ = {ctx, std::move(hsvPoints), drawMode};
	addText(hsvCenter + textOff, "Using per-point color");

	drawMode.aaFill = true;
	drawMode.aaStroke = true;
	drawMode.stroke = 2.5f;
	drawMode.color = {};
	circle_ = {ctx, circlePos, circleRad, drawMode};
	addText(circlePos + textOff, "Anti aliasing & using a texture");
}

void App::update(double dt) {
	pendulum_.update(dt);
}

void App::resize(Vec2ui size) {
	// setup a matrix that will transform from window coords to vulkan coords
	auto mat = nytl::identity<4, float>();
	auto s = nytl::Vec {2.f / size.x, 2.f / size.y, 1};
	scale(mat, s);
	translate(mat, {-1, -1, 0});
	mat = mat * rotMat(angle_);
	mat[0] *= scale_.x;
	mat[1] *= scale_.y;
	*transform_.change() = mat;
	size_ = size;

	auto textWidth = bottomText_.width();
	auto tchange = bottomText_.change();
	tchange->position.x = (size[0] - textWidth) / 2;
	tchange->position.y = size[1] - bottomText_.height() - 20;
}

void App::draw(vk::CommandBuffer cb) {
	transform_.bind(cb);
	paint_.bind(cb);

	gradWidget_.draw(cb);
	path_.draw(cb);

	gradientPaint_.bind(cb);
	starShape_.fill(cb);

	paint_.bind(cb);
	circle_.stroke(cb);
	texturePaint_.bind(cb);
	circle_.fill(cb);

	ctx_.pointColorPaint().bind(cb);
	hsvCircle_.fill(cb);

	paint_.bind(cb);
	for(auto& t : texts_) {
		t.draw(cb);
	}

	pendulum_.draw(cb);

	paint_.bind(cb);
	bottomText_.draw(cb);
}

void App::clicked(Vec2f pos) {
	auto in = [&](Vec2f p, Vec2f size) {
		using namespace nytl::vec::cw;
		return pos == clamp(pos, p, p + size);
	};

	auto h = bottomText_.height();
	if(in(bottomText_.position(), {bottomText_.width(), h})) {
		// opens the github link in browser
		// ikr, std::system isn't a good choice, generally.
		// But here, i feel like it's enough
#ifdef RVG_EXAMPLE_UNIX
		std::system("xdg-open https://www.github.com/nyorain/rvg");
#elif defined(RVG_EXAMPLE_WIN)
		// https://stackoverflow.com/questions/3739327
		std::system("explorer https://www.github.com/nyorain/rvg");
#endif
	} else if(in(path_.pos(), path_.size())) {
		path_.clicked(pos);
	}
}

void App::key(unsigned key, bool pressed) {
	if(key == GLFW_KEY_LEFT) {
		pendulum_.left(pressed);
	} else if(key == GLFW_KEY_RIGHT) {
		pendulum_.right(pressed);
	}

	if(!pressed) {
		return;
	}

	// TODO
	auto t = false;

	if(key == GLFW_KEY_B) {
		*paint_.change() = rvg::colorPaint({rvg::norm, 0.2, 0.2, 0.8});
	} else if(key == GLFW_KEY_G) {
		*paint_.change() = rvg::colorPaint({rvg::norm, 0.1, 0.6, 0.3});
	} else if(key == GLFW_KEY_R) {
		*paint_.change() = rvg::colorPaint({rvg::norm, 0.8, 0.2, 0.3});
	} else if(key == GLFW_KEY_D) {
		*paint_.change() = rvg::colorPaint({rvg::norm, 0.1, 0.1, 0.1});
	} else if(key == GLFW_KEY_W) {
		*paint_.change() = rvg::colorPaint(rvg::Color::white);
	} else if(key == GLFW_KEY_P) {
		*paint_.change() = rvg::linearGradient({0, 0}, {2000, 1000},
			{255, 0, 0}, {255, 255, 0});
	} else if(key == GLFW_KEY_C) {
		*paint_.change() = rvg::radialGradient({1000, 500}, 0, 1000,
			{255, 0, 0}, {255, 255, 0});
	}

	else if(key == GLFW_KEY_Q) {
		angle_ += 0.1;
		t = true;
	} else if(key == GLFW_KEY_E) {
		angle_ -= 0.1;
		t = true;
	} else if(key == GLFW_KEY_X) {
		scale_.x *= 1.1;
		t = true;
	} else if(key == GLFW_KEY_Y) {
		scale_.y *= 1.1;
		t = true;
	} else if(key == GLFW_KEY_I) {
		scale_ *= 1.1;
		t = true;
	} else if(key == GLFW_KEY_O) {
		scale_ *= 1.f / 1.1;
		t = true;
	}

	if(key == GLFW_KEY_H) {
		auto d = !path_.shape().disabled();
		path_.shape().disable(d);
	}

	if(t) {
		auto size = size_;
		auto mat = nytl::identity<4, float>();
		auto s = nytl::Vec {2.f / size.x, 2.f / size.y, 1};
		scale(mat, s);
		translate(mat, {-1, -1, 0});
		mat = mat * rotMat(angle_);
		mat[0][0] *= scale_.x;
		mat[1][1] *= scale_.y;
		*transform_.change() = mat;
	}
}

// main
int main() {
	// - initialization -
	if(!::glfwInit()) {
		throw std::runtime_error("Failed to init glfw");
	}

	// vulkan init: instance
	uint32_t count;
	const char** extensions = ::glfwGetRequiredInstanceExtensions(&count);

	std::vector<const char*> iniExtensions {extensions, extensions + count};
	iniExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	vk::ApplicationInfo appInfo (appName, 1, engineName, 1, VK_API_VERSION_1_0);
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = &appInfo;

	instanceInfo.enabledExtensionCount = iniExtensions.size();
	instanceInfo.ppEnabledExtensionNames = iniExtensions.data();

	if(useValidation) {
		auto layers = {
			layerName,
		};

		instanceInfo.enabledLayerCount = layers.size();
		instanceInfo.ppEnabledLayerNames = layers.begin();
	}

	vpp::Instance instance {};
	try {
		instance = {instanceInfo};
		if(!instance.vkInstance()) {
			throw std::runtime_error("vkCreateInstance returned a nullptr");
		}
	} catch(const std::exception& error) {
		dlg_error("Vulkan instance creation failed: {}", error.what());
		dlg_error("\tYour system may not support vulkan");
		dlg_error("\tThis application requires vulkan to work");
		throw;
	}

	// debug callback
	std::unique_ptr<vpp::DebugMessenger> debugCallback;
	if(useValidation) {
		debugCallback = std::make_unique<vpp::DebugMessenger>(instance);
	}

	// init glfw window
	const auto size = nytl::Vec {1200u, 800u};
	::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = ::glfwCreateWindow(size.x, size.y, "rvg", NULL, NULL);
	if(!window) {
		throw std::runtime_error("Failed to create glfw window");
	}

	// avoiding reinterpret_cast due to aliasing warnings
	VkInstance vkini;
	auto handle = instance.vkHandle();
	std::memcpy(&vkini, &handle, sizeof(vkini));
	static_assert(sizeof(VkInstance) == sizeof(vk::Instance));

	VkSurfaceKHR vkSurf {};
	VkResult err = ::glfwCreateWindowSurface(vkini, window, NULL, &vkSurf);
	if(err) {
		auto str = std::string("Failed to create vulkan surface: ");
		str += vk::name(static_cast<vk::Result>(err));
		throw std::runtime_error(str);
	}

	vk::SurfaceKHR surface {};
	std::memcpy(&surface, &vkSurf, sizeof(surface));
	static_assert(sizeof(VkSurfaceKHR) == sizeof(vk::SurfaceKHR));

	// create device
	// enable some extra features
	float priorities[1] = {0.0};

	auto phdevs = vk::enumeratePhysicalDevices(instance);
	auto phdev = vpp::choose(phdevs, surface);

	auto queueFlags = vk::QueueBits::compute | vk::QueueBits::graphics;
	int queueFam = vpp::findQueueFamily(phdev, surface, queueFlags);

	vk::DeviceCreateInfo devInfo;
	vk::DeviceQueueCreateInfo queueInfo({}, queueFam, 1, priorities);

	auto exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	devInfo.pQueueCreateInfos = &queueInfo;
	devInfo.queueCreateInfoCount = 1u;
	devInfo.ppEnabledExtensionNames = exts.begin();
	devInfo.enabledExtensionCount = 1u;

	auto features = vk::PhysicalDeviceFeatures {};
	features.shaderClipDistance = true;
	devInfo.pEnabledFeatures = &features;

	auto device = vpp::Device(instance, phdev, devInfo);
	auto presentQueue = device.queue(queueFam);

	auto renderInfo = RendererCreateInfo {
		device, surface, size, *presentQueue,
		startMsaa, vsync, clearColor
	};

	// optional so we can manually destroy it before vulkan surface
	auto orenderer = std::optional<Renderer>(renderInfo);
	auto& renderer = *orenderer;

	// app
	rvg::Context ctx(device, {renderer.renderPass(), 0, true});
	App app(ctx);

	// render recoreding
	renderer.onRender += [&](vk::CommandBuffer cb){
		ctx.bindDefaults(cb);
		app.draw(cb);
	};

	ctx.updateDevice();
	renderer.invalidate();

	// connect window & renderer
	struct WinInfo {
		Renderer* renderer;
		App* app;
	} winInfo = {
		&renderer,
		&app,
	};

	::glfwSetWindowUserPointer(window, &winInfo);

	auto sizeCallback = [](auto* window, int width, int height) {
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		auto size = nytl::Vec {unsigned(width), unsigned(height)};
		winInfo.renderer->resize(size);
		winInfo.app->resize(size);
	};

	auto keyCallback = [](auto* window, int key, int, int action, int) {
		auto pressed = action != GLFW_RELEASE;
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		winInfo.app->key(key, pressed);
	};

	auto mouseCallback = [](auto* window, int button, int action, int) {
		auto ptr = ::glfwGetWindowUserPointer(window);
		const auto& winInfo = *static_cast<const WinInfo*>(ptr);
		auto pressed = action == GLFW_PRESS;
		if(pressed && button == GLFW_MOUSE_BUTTON_LEFT) {
			double x,y;
			::glfwGetCursorPos(window, &x, &y);
			winInfo.app->clicked({float(x), float(y)});
		}
	};

	::glfwSetFramebufferSizeCallback(window, sizeCallback);
	::glfwSetKeyCallback(window, keyCallback);
	::glfwSetMouseButtonCallback(window, mouseCallback);

	// - main loop -
	using Clock = std::chrono::high_resolution_clock;
	using Secf = std::chrono::duration<float, std::ratio<1, 1>>;

	auto lastFrame = Clock::now();
	auto fpsCounter = 0u;
	auto secCounter = 0.f;

	while(!::glfwWindowShouldClose(window)) {
		auto now = Clock::now();
		auto diff = now - lastFrame;
		auto dt = std::chrono::duration_cast<Secf>(diff).count();
		lastFrame = now;

		app.update(dt);
		::glfwPollEvents();

		auto [rec, seph] = ctx.upload();

		if(rec) {
			dlg_info("Rerecording due to context");
			renderer.invalidate();
		}

		auto wait = {
			vpp::StageSemaphore {
				seph,
				vk::PipelineStageBits::allGraphics,
			}
		};

		vpp::RenderInfo info;
		if(seph) {
			info.wait = wait;
		}

		renderer.renderStall(info);

		if(printFrames) {
			++fpsCounter;
			secCounter += dt;
			if(secCounter >= 1.f) {
				dlg_info("{} fps", fpsCounter);
				secCounter = 0.f;
				fpsCounter = 0;
			}
		}
	}

	orenderer.reset();
	vkDestroySurfaceKHR(vkini, vkSurf, nullptr);
	::glfwDestroyWindow(window);
	::glfwTerminate();
}
