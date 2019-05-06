// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/vec.hpp>
#include <nytl/mat.hpp>
#include <nytl/stringParam.hpp>
#include <vpp/trackedDescriptor.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/image.hpp>

#include <cstdint>

namespace rvg {

// - Color -
using u8 = std::uint8_t;
constexpr struct Norm {} norm;

/// RGBA32 color tuple.
/// Offers various utility and conversion functions below.
/// rvg (e.g. the Paint class) interprets the given data as in
/// the srgb colorspace (i.e. the same way html/css do it).
/// So when using a color here it should appear the same
/// way on the screen as in would in a browser (given that you
/// use a srgb framebuffer or otherwise manually gamma correct).
/// Therefore color values should NEVER be directly used
/// for blending/mixing. Use the utility functions below, or convert
/// them to linear rgb space before.
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

// operators
inline bool operator==(Color a, Color b) { return a.rgba() == b.rgba(); }
inline bool operator!=(Color a, Color b) { return a.rgba() != b.rgba(); }

// srgb rgb (gamma-based approximations, you might need real conversion!)
// will not convert alpha range
// returns Vec4f to not lose precision for dark colors.
Vec4f linearize(const Color&, float gamma = 2.2);
Color srgb(Vec4f rgbLinearNorm, float gamma = 2.2);

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

/// Conceptuablly returns (fac * a + (1 - fac) * b), i.e. mixes the given
/// colors with the given factor (which should be in range [0,1]).
/// But since all colors in rvg are srgb, will linearize them before
/// mixing (and convert back to srgb after), which is the right way
/// for gradients (NOTE: this is different from css since css "does"
/// it wrong).
Color mix(const Color& a, const Color& b, float fac);


// - Texture -
/// Type (format) of a texture.
enum class TextureType {
	rgba32,
	a8
};

/// Texture on the device.
class Texture : public DeviceObject {
public:
	using Type = TextureType;

public:
	Texture() = default;
	~Texture() = default;

	Texture(Texture&&) noexcept = default;
	Texture& operator=(Texture&&) noexcept = default;

	/// Attempts to load the texture from the given file.
	/// Throws std::runtime_error if the file cannot be loaded.
	/// The passed type will be forced.
	Texture(Context&, nytl::StringParam filename, Type = Type::rgba32);

	/// Creates the texture with given size, data and type.
	/// data must reference at least size.x * size.y * formatSize(type) bytes,
	/// where formatSize(Type::a8) = 1 and formatSize(Type::rgba32) = 4.
	Texture(Context&, Vec2ui size, nytl::Span<const std::byte> data, Type);

	/// Updates the given texture with the given data.
	/// data must reference at least size.x * size.y * formatSize(type()) bytes,
	/// where formatSize(Type::a8) = 1 and formatSize(Type::rgba32) = 4.
	void update(std::vector<std::byte> data);

	const auto& viewableImage() const { return image_; }
	const auto& size() const { return size_; }
	auto vkImage() const { return viewableImage().vkImage(); }
	auto vkImageView() const { return viewableImage().vkImageView(); }
	auto type() const { return type_; }

	bool updateDevice(std::vector<std::byte> data);
	bool updateDevice();

protected:
	void create();
	void upload(nytl::Span<const std::byte> data, vk::ImageLayout);

protected:
	vpp::ViewableImage image_;
	Vec2ui size_ {};
	Type type_ {};
	std::vector<std::byte> pending_;
};


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
	Paint(Context&, const PaintData& data, bool deviceLocal = true);

	/// Binds the Paint object in the given DrawInstance.
	/// Following calls to fill/stroke of a polygon-based shape
	/// or Text::draw will use this bound paint until another
	/// Paint is bound.
	void bind(vk::CommandBuffer cb) const;

	auto change() { return StateChange {*this, paint_}; }
	void paint(const PaintData& data) { *change() = data; }
	const auto& paint() const { return paint_; }

	const auto& ubo() const { return ubo_; }
	const auto& ds() const { return ds_; }

	void update();
	bool updateDevice();

protected:
	void upload();

protected:
	PaintData paint_ {};
	vpp::SubBuffer ubo_;
	vpp::TrDs ds_;
	vk::ImageView oldView_ {};
};

} // namespace rvg
