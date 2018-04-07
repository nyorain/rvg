// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/vec.hpp>
#include <nytl/mat.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/sharedBuffer.hpp>

#include <cstdint>

namespace rvg {

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


// - Texture -
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
	vpp::SubBuffer ubo_;
	vpp::DescriptorSet ds_;
	vk::ImageView oldView_ {};
};

} // namespace rvg
