#include <vgv/vgv.hpp>
#include <vpp/vk.hpp>
#include <dlg/dlg.hpp>
#include <nytl/matOps.hpp>

namespace vgv {
namespace {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

} // anon namespace

PaintData colorPaint(const Color& color) {
	PaintData ret;
	ret.data.frag.type = PaintType::color;
	ret.data.frag.inner = color;
	return ret;
}

PaintData linearGradient(Vec2f start, Vec2f end,
		const Color& startColor, const Color& endColor) {
	auto d = end - start;
	auto l = length(d);
	d = l > 0.001f ? (1 / l) * d : Vec {0.f, 1.f};

	auto large = 1e5f;

	PaintData ret;
	ret.data.transform = nytl::identity<4, float>();
	ret.data.transform[0][0] = d.y;
	ret.data.transform[0][1] = d.x;
	ret.data.transform[1][0] = d.x;
	ret.data.transform[1][1] = d.y;
	ret.data.transform[0][3] = start.x - d.x * large;
	ret.data.transform[1][3] = start.y - d.y * large;

	ret.data.frag.inner = startColor;
	ret.data.frag.outer = endColor;
	ret.data.frag.radius = 0.0f;
	ret.data.frag.feather = std::max(1.f, l);
	ret.data.frag.extent = {large, large + l * 0.5f};
	ret.data.frag.type = PaintType::gradient;

	return ret;
}

PaintData radialGradient(Vec2f center, float innerRadius, float outerRadius,
		const Color& innerColor, const Color& outerColor) {
	auto r = (innerRadius + outerRadius) * 0.5f;

	PaintData ret;
	ret.data.transform = nytl::identity<4, float>();
	ret.data.transform[0][3] = center.x;
	ret.data.transform[1][3] = center.y;

	ret.data.frag.type = PaintType::gradient;
	ret.data.frag.radius = r;
	ret.data.frag.extent = {r, r};
	ret.data.frag.feather = std::max(1.f, outerRadius - innerRadius);
	ret.data.frag.inner = innerColor;
	ret.data.frag.outer = outerColor;

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
Paint::Paint(Context& ctx, const PaintData& xpaint) : paint(xpaint) {
	if(!paint.texture) {
		paint.texture = ctx.emptyImage().vkImageView();
	}

	oldView_ = paint.texture;
	ubo_ = ctx.device().bufferAllocator().alloc(true,
		sizeof(DevicePaintData), vk::BufferUsageBits::uniformBuffer);

	ds_ = {ctx.dsLayoutPaint(), ctx.dsPool()};
	auto map = ubo_.memoryMap();
	auto ptr = map.ptr();
	write(ptr, paint.data);

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
	write(ptr, paint.data);

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
