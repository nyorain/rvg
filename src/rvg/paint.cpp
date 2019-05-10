// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/util.hpp>
#include <rvg/paint.hpp>
#include <rvg/context.hpp>

#include <vpp/vk.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop

// color conversions sources:
// https://stackoverflow.com/questions/2353211/
// http://en.wikipedia.org/wiki/HSL_color_space
// https://codeitdown.com/hsl-hsb-hsv-color/
// https://stackoverflow.com/questions/3423214/
// https://stackoverflow.com/questions/3018313/

namespace rvg {
namespace {

template<typename... Args>
bool normed(Args&&... args) {
	return ((args >= 0.f && args <= 1.f) && ...);
}

float hue2rgb(float p, float q, float t) {
	if(t < 0) t += 1;
	if(t > 1) t -= 1;
	if(t < 1/6.f) return p + (q - p) * 6 * t;
	if(t < 1/2.f) return q;
	if(t < 2/3.f) return p + (q - p) * (2/3.f - t) * 6;
	return p;
}

} // anon namespace

// Color
const Color Color::white {255, 255, 255};
const Color Color::black {0, 0, 0};
const Color Color::red {255, 0, 0};
const Color Color::green {0, 255, 0};
const Color Color::blue {0, 0, 255};

Color::Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a) {
}

Color::Color(Vec3u8 rgb, u8 a) : r(rgb.x), g(rgb.y), b(rgb.z), a(a) {
}

Color::Color(Vec4u8 rgba) : r(rgba[0]), g(rgba[1]), b(rgba[2]), a(rgba[3]) {
}

Color::Color(Norm, float r, float g, float b, float a)
		: r(255.f * r), g(255.f * g), b(255.f * b), a(255.f * a) {
	dlg_assert(normed(r, g, b, a));
}

Color::Color(Norm, Vec3f rgb, float a)
	: Color(norm, rgb[0], rgb[1], rgb[2], a) {
	dlg_assert(normed(rgb[0], rgb[1], rgb[2], a));
}

Color::Color(Norm, Vec4f rgba)
		: Color(norm, rgba[0], rgba[1], rgba[2], rgba[3]) {
	dlg_assert(normed(rgba[0], rgba[1], rgba[2], rgba[3]));
}

Vec3f Color::rgbNorm() const {
	return (1 / 255.f) * rgb();
}

Vec4f Color::rgbaNorm() const {
	return (1 / 255.f) * rgba();
}

// - hsl -
Color hsl(u8 h, u8 s, u8 l, u8 a) {
	return hslNorm(255.f * h, 255.f * s, 255.f * l, 255.f * a);
}

Color hslNorm(float h, float s, float l, float a) {
	dlg_assert(normed(h, s, l, a));
	if(s == 0.f) {
		return {norm, l, l, l, a};
	}

	auto q = l < 0.5 ? l * (1 + s) : l + s - l * s;
	auto p = 2 * l - q;
	return {norm,
		hue2rgb(p, q, h + 1/3.f),
		hue2rgb(p, q, h),
		hue2rgb(p, q, h - 1/3.f), a};
}

Vec3u8 hsl(const Color& c) {
	return Vec3u8(255.f * hslNorm(c));
}

Vec4u8 hsla(const Color& c) {
	auto h = hsl(c);
	return {h.x, h.y, h.z, c.a};
}

Vec3f hslNorm(const Color& c) {
	auto [r, g, b] = c.rgbNorm();

	auto min = std::min(r, std::min(g, b));
	auto max = std::max(r, std::max(g, b));
	auto l = (max + min) / 2;

    if(max == min){
		return {0.f, 0.f, l};
    }

	auto h = 0.f;
	auto d = max - min;
	auto s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
	if(max == r) {
		h = (g - b) / d + (g < b ? 6 : 0);
	} else if(max == g) {
		h = (b - r) / d + 2;
	} else {
		h = (r - g) / d + 4;
	}
	h /= 6;

    return {h, s, l};
}

Vec4f hslaNorm(const Color& c) {
	auto h = hslNorm(c);
	return {h.x, h.y, h.z, 255.f * c.a};
}

// - hsv -
Color hsv(u8 h, u8 s, u8 v, u8 a) {
	return hsvNorm(h / 255.f, s / 255.f, v / 255.f, a / 255.f);
}

Color hsvNorm(float h, float s, float v, float a) {
	dlg_assertm(normed(h, s, v, a), "{} {} {} {}", h, s, v, a);
	if(s == 0) {
		return {norm, v, v, v, a};
	}

	auto hh = std::fmod((h * 6.f), 6.f);
	auto i = static_cast<unsigned>(hh);
	auto ff = hh - i;
	auto p = v * (1.f - s);
    auto q = v * (1.f - (s * ff));
    auto t = v * (1.f - (s * (1.f - ff)));

    switch(i) {
		case 0: return {norm, v, t, p, a};
		case 1: return {norm, q, v, p, a};
		case 2: return {norm, p, v, t, a};
		case 3: return {norm, p, q, v, a};
		case 4: return {norm, t, p, v, a};
		default: return {norm, v, p, q, a};
    }
}

Vec3u8 hsv(const Color& c) {
	return Vec3u8(255 * hsvNorm(c));
}

Vec4u8 hsva(const Color& c) {
	return Vec4u8(255 * hsvaNorm(c));
}

Vec3f hsvNorm(const Color& c) {
	auto hsva = hsvaNorm(c);
	return {hsva[0], hsva[1], hsva[2]};
}

Vec4f hsvaNorm(const Color& c) {
	auto [r, g, b] = c.rgbNorm();

	auto min = std::min(r, std::min(g, b));
	auto max = std::max(r, std::max(g, b));

    if(max == min){
		return {0.f, 0.f, max}; // h could be NaN
    }

	float h;
	auto d = max - min;
	auto s = d / max;
	if(max == r) {
		h = (g - b) / d + (g < b ? 6 : 0);
	} else if(max == g) {
		h = (b - r) / d + 2;
	} else {
		h = (r - g) / d + 4;
	}
	h /= 6;

    return {h, s, max, c.a / 255.f};
}

// - hsv/hsl conversion -
Vec3f hsl2hsv(Vec3f hsl) {
	dlg_assert(normed(hsl.x, hsl.y, hsl.z));
	auto t = hsl[1] * 0.5f * (1 - std::abs(2 * hsl[2] - 1));
	auto v = hsl[2] + t;
  	auto s = hsl[2] > 0 ? 2 * t / v : 0.f;

	return {hsl[0], s, v};
}

Vec3f hsv2hsl(Vec3f hsv) {
	dlg_assert(normed(hsv.x, hsv.y, hsv.z));
	auto l = 0.5f * hsv.z * (2 - hsv.z);
	auto s = hsv[1];
	if(l > 0 && l < 1) {
		s = hsv[1] * hsv[2] / (1 - std::abs(2 * l - 1));
	}

	return {hsv[0], s, l};
}

// - further util -
Color u32rgba(std::uint32_t val) {
	return {
		u8((val & 0xFF000000u) >> 24),
		u8((val & 0x00FF0000u) >> 16),
		u8((val & 0x0000FF00u) >> 8),
		u8((val & 0x000000FFu) >> 0)};
}

std::uint32_t u32rgba(const Color& c) {
	return c.r << 24 | c.g << 16 | c.b << 8 | c.a;
}

Color mix(const Color& a, const Color& b, float fac) {
	// gamma-correct
	auto la = linearize(a);
	auto lb = linearize(b);
	return srgb(fac * la + (1 - fac) * lb);

	// gamma-incorrect
	// return {norm, fac * a.rgbaNorm() + (1 - fac) * b.rgbaNorm()};
}

// Paint functions
PaintData colorPaint(const Color& color) {
	PaintData ret;
	ret.data.frag.type = PaintType::color;
	ret.data.frag.inner = color;
	return ret;
}

PaintData linearGradient(Vec2f start, Vec2f end,
		const Color& startColor, const Color& endColor) {
	PaintData ret;
	ret.data.transform = nytl::identity<4, float>();

	ret.data.frag.type = PaintType::linGrad;
	ret.data.frag.inner = startColor;
	ret.data.frag.outer = endColor;
	ret.data.frag.custom = {start.x, start.y, end.x, end.y};

	return ret;
}

PaintData radialGradient(Vec2f center, float innerRadius, float outerRadius,
		const Color& innerColor, const Color& outerColor) {
	PaintData ret;
	ret.data.transform = nytl::identity<4, float>();

	ret.data.frag.type = PaintType::radGrad;
	ret.data.frag.inner = innerColor;
	ret.data.frag.outer = outerColor;
	ret.data.frag.custom = {center.x, center.y, innerRadius, outerRadius};

	return ret;
}

PaintData texturePaintRGBA(const nytl::Mat4f& transform, vk::ImageView iv) {
	PaintData ret;
	ret.texture = iv;
	ret.data.transform = transform;
	ret.data.frag.inner = Color::white;
	ret.data.frag.type = PaintType::textureRGBA;
	return ret;
}

PaintData texturePaintA(const nytl::Mat4f& transform, vk::ImageView iv) {
	PaintData ret;
	ret.texture = iv;
	ret.data.transform = transform;
	ret.data.frag.type = PaintType::textureA;
	ret.data.frag.inner = Color::white;
	return ret;
}

PaintData pointColorPaint() {
	PaintData ret;
	ret.data.frag.type = PaintType::pointColor;
	return ret;
}

// Paint
constexpr auto paintUboSize = sizeof(nytl::Mat4f) + sizeof(Vec4f) * 3 + 4;
Paint::Paint(Context& ctx, const PaintData& xpaint, bool deviceLocal) :
		DeviceObject(ctx), paint_(xpaint) {

	if(!paint_.texture) {
		paint_.texture = ctx.emptyImage().vkImageView();
	}

	oldView_ = paint_.texture;
	auto usage = nytl::Flags{vk::BufferUsageBits::uniformBuffer};
	if(deviceLocal) {
		usage |= vk::BufferUsageBits::transferDst;
	}
	auto memBits = deviceLocal ?
		context().device().deviceMemoryTypes() :
		context().device().hostMemoryTypes();
	auto align = std::max(vk::DeviceSize(16u),
		ctx.device().properties().limits.minUniformBufferOffsetAlignment);
	ubo_ = {ctx.bufferAllocator(), paintUboSize, usage, align, memBits};

	ds_ = {ctx.dsAllocator(), ctx.dsLayoutPaint()};
	upload();

	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
	update.imageSampler({{{}, paint_.texture,
		vk::ImageLayout::shaderReadOnlyOptimal}});
}

void Paint::update() {
	dlg_assert(valid() && ds_ && ubo_.size());
	context().registerUpdateDevice(this);
}

void Paint::upload() {
	dlg_assert(valid() && ubo_.size());

	// NOTE: really important part here!
	// shaders always work on linear colors (since operating on
	// colors in gamma-corrected srgb space gives wrong results).
	// But since we expect srgb color values from the user (we do
	// that since that's what people are expected to work with;
	// otherwise they e.g. would have way less dark colors
	// to work with) we have to linearize them.
	// Could also be done in the shader, but here is more efficient.
	// It's also important that we get float values instead of
	// u8 since with u8 we would lose the dark-color precision
	// we need (and users expect; can actually see).
	auto inner = linearize(paint_.data.frag.inner);
	auto outer = linearize(paint_.data.frag.outer);

	upload140(*this, ubo_,
		vpp::raw(paint_.data.transform),
		vpp::raw(inner),
		vpp::raw(outer),
		vpp::raw(paint_.data.frag.custom),
		vpp::raw(static_cast<std::uint32_t>(paint_.data.frag.type)));
}

void Paint::bind(vk::CommandBuffer cb) const {
	dlg_assert(valid() && ds_ && ubo_.size());
	vk::cmdBindDescriptorSets(cb, vk::PipelineBindPoint::graphics,
		context().pipeLayout(), Context::paintBindSet, {ds_}, {});
}

bool Paint::updateDevice() {
	dlg_assert(valid() && ds_ && ubo_.size());
	auto re = false;
	if(!paint_.texture) {
		paint_.texture = context().emptyImage().vkImageView();
	}

	upload();

	if(oldView_ != paint_.texture) {
		vpp::DescriptorSetUpdate update(ds_);
		update.skip(2);
		update.imageSampler({{{}, paint_.texture,
			vk::ImageLayout::shaderReadOnlyOptimal}});
		oldView_ = paint_.texture;
		re = true;
	}

	return re;
}

Vec4f linearize(const Color& c, float gamma) {
	auto r = Vec4f(nytl::vec::cw::pow(c.rgbNorm(), gamma));
	r[3] = c.a / 255.f;
	return r;
}

Color srgb(Vec4f rgbLinearNorm, float gamma) {
	return {norm,
		nytl::vec::cw::pow(Vec3f(rgbLinearNorm), 1 / gamma),
		rgbLinearNorm[3]};
}

// Texture
Texture::Texture(Context& ctx, nytl::StringParam filename, Type type) :
		DeviceObject(ctx), type_(type) {

	int width, height, channels;
	unsigned char* data = stbi_load(filename.c_str(), &width, &height,
		&channels, 4);
	if(!data) {
		dlg_warn("Failed to open texture file {}", filename);

		std::string err = "Could not load image from ";
		err += filename;
		err += ": ";
		err += stbi_failure_reason();
		throw std::runtime_error(err);
	}

	if((channels == 1 || channels == 3) && type == TextureType::a8) {
		dlg_warn("Creating a8 texture from alpha-less image");
	}

	dlg_assert(width > 0 && height > 0);
	std::vector<std::byte> alphaData;
	auto ptr = reinterpret_cast<const std::byte*>(data);
	size_t dataSize = width * height * 4u;
	if(type == TextureType::a8) {
		alphaData.resize(width * height);
		ptr = alphaData.data();
		dataSize /= 4;
		for(auto i = 0u; i < unsigned(width * height); ++i) {
			alphaData[i] = std::byte {data[4 * i + 3]};
		}
	}

	size_.x = width;
	size_.y = height;
	create();
	upload({ptr, dataSize}, vk::ImageLayout::undefined);
	free(data);
}

Texture::Texture(Context& ctx, Vec2ui size, nytl::Span<const std::byte> data,
		Type type) : DeviceObject(ctx), size_(size), type_(type) {
	create();
	upload(data, vk::ImageLayout::undefined);
}

void Texture::create() {
	constexpr auto usage =
		vk::ImageUsageBits::transferDst |
		vk::ImageUsageBits::sampled;

	vk::Extent3D extent;
	extent.width = size_.x;
	extent.height = size_.y;
	extent.depth = 1u;

	vpp::ViewableImageCreateInfo info;
	auto dataSize = extent.width * extent.height;
	auto& dev = context().device();

	if(type() == TextureType::rgba32) {
		dataSize = dataSize * 4;
		info = vpp::ViewableImageCreateInfo::color(dev, extent, usage,
			{vk::Format::r8g8b8a8Srgb}).value();
	} else if(type() == TextureType::a8) {
		info = vpp::ViewableImageCreateInfo::color(dev, extent, usage,
			{vk::Format::r8Unorm}).value();
		info.view.components = {
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::r,
		};
	} else {
		throw std::runtime_error("Texture: Invalid rvg::TextureType");
	}

	info.img.imageType = vk::ImageType::e2d;
	info.view.viewType = vk::ImageViewType::e2d;
	auto memBits = dev.memoryTypeBits(vk::MemoryPropertyBits::deviceLocal);

	image_ = {dev, info, memBits};
}

void Texture::upload(nytl::Span<const std::byte> data, vk::ImageLayout layout) {
	auto cmdBuf = context().uploadCmdBuf();
	vpp::changeLayout(cmdBuf, image_.image(),
		layout, vk::PipelineStageBits::topOfPipe, {},
		vk::ImageLayout::transferDstOptimal, vk::PipelineStageBits::transfer,
		vk::AccessBits::transferWrite,
		{vk::ImageAspectBits::color, 0, 1, 0, 1});
	layout = vk::ImageLayout::transferDstOptimal;

	auto size = vk::Extent3D{size_.x, size_.y, 1u};
	auto format = type_ == Type::a8 ?
		vk::Format::r8Unorm : // TODO: use srgb here as well?
		vk::Format::r8g8b8a8Srgb;
	auto stage = vpp::fillStaging(cmdBuf, image_.image(), format, layout,
		size, data, {vk::ImageAspectBits::color});

	vpp::changeLayout(cmdBuf, image_.image(),
		layout, vk::PipelineStageBits::transfer,
		vk::AccessBits::transferWrite,
		vk::ImageLayout::shaderReadOnlyOptimal,
		vk::PipelineStageBits::allGraphics,
		vk::AccessBits::shaderRead,
		{vk::ImageAspectBits::color, 0, 1, 0, 1});

	context().addStage(std::move(stage));
	context().addCommandBuffer(this, std::move(cmdBuf));
}

void Texture::update(std::vector<std::byte> data) {
	dlg_assert(size_.x * size_.y * (type_ == Type::a8 ? 1u : 4u) == data.size());
	pending_ = std::move(data);
	context().registerUpdateDevice(this);
}

bool Texture::updateDevice(std::vector<std::byte> data) {
	dlg_assert(size_.x * size_.y * (type_ == Type::a8 ? 1u : 4u) == data.size());
	pending_ = std::move(data);
	return updateDevice();
}

bool Texture::updateDevice() {
	upload(pending_, vk::ImageLayout::shaderReadOnlyOptimal);
	return false;
}

} // namespace vgv
