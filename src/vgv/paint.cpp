#include <vgv/vgv.hpp>
#include <vpp/vk.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>

// Conversions source: stackoverflow and wikipedia
// https://stackoverflow.com/questions/2353211/hsl-to-rgb-color-conversion
// http://en.wikipedia.org/wiki/HSL_color_space

namespace vgv {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

float hue2rgb(float p, float q, float t) {
	if(t < 0) t += 1;
	if(t > 1) t -= 1;
	if(t < 1/6.f) return p + (q - p) * 6 * t;
	if(t < 1/2.f) return q;
	if(t < 2/3.f) return p + (q - p) * (2/3.f - t) * 6;
	return p;
}

void uploadPaint(std::byte*& ptr, const DevicePaintData& data) {
	write(ptr, data.transform);
	write(ptr, data.frag.inner.rgbaNorm());
	write(ptr, data.frag.outer.rgbaNorm());
	write(ptr, data.frag.custom);
	write(ptr, data.frag.type);
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
}

Color::Color(Norm, Vec3f rgb, float a)
	: Color(norm, rgb[0], rgb[1], rgb[2], a) {
}

Color::Color(Norm, Vec4f rgba)
	: Color(norm, rgba[0], rgba[1], rgba[2], rgba[3]) {
}

Vec3f Color::rgbNorm() const {
	return (1 / 255.f) * rgb();
}

Vec4f Color::rgbaNorm() const {
	return (1 / 255.f) * rgba();
}

Color hsl(u8 h, u8 s, u8 l, u8 a) {
	return hslNorm(255.f * h, 255.f * s, 255.f * l, 255.f * a);
}

Color hslNorm(float h, float s, float l, float a) {
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

	float h;
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

Color u32rgba(std::uint32_t val) {
	return {
		u8(val & 0xFF000000),
		u8(val & 0x00FF0000),
		u8(val & 0x0000FF00),
		u8(val & 0x000000FF)};
}

std::uint32_t u32rgba(const Color& c) {
	return c.r << 24 | c.g << 16 | c.b << 8 | c.a;
}

Color mix(const Color& a, const Color& b, float fac) {
	return static_cast<Vec3u8>(fac * a.rgba() + (1 - fac) * b.rgba());
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
	ret.data.frag.type = PaintType::textureRGBA;
	return ret;
}

PaintData texturePaintA(const nytl::Mat4f& transform, vk::ImageView iv) {
	PaintData ret;
	ret.texture = iv;
	ret.data.transform = transform;
	ret.data.frag.type = PaintType::textureA;
	return ret;
}

// Paint
constexpr auto paintUboSize = sizeof(nytl::Mat4f) + sizeof(Vec4f) * 3 + 4;
Paint::Paint(Context& ctx, const PaintData& xpaint) : paint(xpaint) {
	if(!paint.texture) {
		paint.texture = ctx.emptyImage().vkImageView();
	}

	oldView_ = paint.texture;
	ubo_ = ctx.device().bufferAllocator().alloc(true,
		paintUboSize, vk::BufferUsageBits::uniformBuffer);

	ds_ = {ctx.dsLayoutPaint(), ctx.dsPool()};
	auto map = ubo_.memoryMap();
	auto ptr = map.ptr();
	uploadPaint(ptr, paint.data);

	vpp::DescriptorSetUpdate update(ds_);
	auto m4 = sizeof(nytl::Mat4f);
	update.uniform({{ubo_.buffer(), ubo_.offset(), m4}});
	update.uniform({{ubo_.buffer(), ubo_.offset() + m4, ubo_.size() - m4}});
	update.imageSampler({{{}, paint.texture, vk::ImageLayout::general}});
}

void Paint::bind(const DrawInstance& di) const {
	dlg_assert(ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), paintBindSet, {ds_}, {});
}

bool Paint::updateDevice(const Context& ctx) {
	if(!paint.texture) {
		paint.texture = ctx.emptyImage().vkImageView();
	}

	auto map = ubo_.memoryMap();
	auto ptr = map.ptr();
	uploadPaint(ptr, paint.data);

	if(oldView_ != paint.texture) {
		vpp::DescriptorSetUpdate update(ds_);
		update.skip(2);
		update.imageSampler({{{}, paint.texture, vk::ImageLayout::general}});
		oldView_ = paint.texture;
		return true;
	}

	return false;
}

} // namespace vgv
