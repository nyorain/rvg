#include <rvg/paint.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <vpp/queue.hpp>
#include <vpp/vk.hpp>
#include <dlg/dlg.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// TODO: when fixed and stable: add to vpp?
// TODO: defer memory allocation and work, make more efficient
// TODO: image uploading not really correct.
// https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTexture.hpp

namespace rvg {

void upload(const vpp::Image& img, vk::Format format,
		vk::ImageLayout layout, const vk::Extent3D& size,
		nytl::Span<const std::byte> data, const vk::ImageSubresource& subres) {

	auto& qs = img.device().queueSubmitter();
	auto cmdBuf = img.device().commandAllocator().get(qs.queue().family());
	vk::beginCommandBuffer(cmdBuf, {});

	// TODO: don't just use general but optimal formats
	vpp::changeLayout(cmdBuf, img,
		layout, vk::PipelineStageBits::topOfPipe, {},
		vk::ImageLayout::general, vk::PipelineStageBits::transfer,
		vk::AccessBits::transferWrite, {vk::ImageAspectBits::color, 0, 1, 0, 1});
	layout = vk::ImageLayout::general;
	auto buf = vpp::fillStaging(cmdBuf, img, format, layout, size, data, subres);
	vk::endCommandBuffer(cmdBuf);

	// TODO: performance! Return the work
	qs.wait(qs.add({{}, {}, {}, 1, &cmdBuf.vkHandle(), {}, {}}));
}

vpp::ViewableImage createTexture(const vpp::Device& dev, unsigned int width,
		unsigned int height, const std::byte* data, TextureType type) {

	vk::Extent3D extent;
	extent.width = width;
	extent.height = height;
	extent.depth = 1u;

	vpp::ViewableImageCreateInfo info;
	auto dataSize = extent.width * extent.height;

	if(type == TextureType::rgba32) {
		dataSize = dataSize * 4;
		info = vpp::ViewableImageCreateInfo::color(dev, extent).value();
	} else if(type == TextureType::a8) {
		info = vpp::ViewableImageCreateInfo::color(dev, extent,
			vk::ImageUsageBits::transferDst |
			vk::ImageUsageBits::sampled,
			{vk::Format::r8Unorm}).value();
		info.view.components = {
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::zero,
			vk::ComponentSwizzle::r,
		};

		dlg_assert(info.view.format == vk::Format::r8Unorm);
		dlg_assert(info.img.format == vk::Format::r8Unorm);
	} else {
		throw std::runtime_error("Invalid TextureType");
	}

	info.img.imageType = vk::ImageType::e2d;
	info.view.viewType = vk::ImageViewType::e2d;
	auto memBits = dev.memoryTypeBits(vk::MemoryPropertyBits::deviceLocal);

	vpp::ViewableImage img {dev, info, memBits};
	upload(img.image(), info.img.format, info.img.initialLayout, extent,
		{data, dataSize}, {vk::ImageAspectBits::color});

	return img;
}

vpp::ViewableImage createTexture(const vpp::Device& dev, const char* filename,
		TextureType type) {

	int width, height, channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
	if(!data) {
		dlg_warn("Failed to open texture file {}", filename);

		std::string err = "Could not load image from ";
		err += filename;
		err += stbi_failure_reason();
		throw std::runtime_error(err);
	}

	if((channels == 1 || channels == 3) && type == TextureType::a8) {
		dlg_warn("Creating a8 texture from alpha-less image");
	}

	dlg_assert(width > 0 && height > 0);
	std::vector<std::byte> alphaData;
	auto ptr = reinterpret_cast<const std::byte*>(data);
	if(type == TextureType::a8) {
		alphaData.resize(width * height);
		ptr = alphaData.data();
		for(auto i = 0u; i < unsigned(width * height); ++i) {
			alphaData[i] = std::byte {data[4 * i + 3]};
		}
	}

	auto tex = createTexture(dev, width, height, ptr, type);
	free(data);
	return tex;
}

} // namespace vgv
