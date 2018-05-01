#define BUGGED_NO_MAIN

#include <bugged.hpp>
#include <vpp/vk.hpp>
#include <vpp/memory.hpp>
#include <vpp/device.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/instance.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/formats.hpp>
#include <vpp/image.hpp>
#include <vpp/framebuffer.hpp>
#include <vpp/debug.hpp>
#include <vpp/physicalDevice.hpp>
#include <dlg/dlg.hpp>
#include <memory>
#include <optional>

class CustomDebugCallback : public vpp::DebugCallback {
public:
	using vpp::DebugCallback::DebugCallback;

	bool call(const CallbackInfo& info) const noexcept override {
		if(info.flags & vk::DebugReportBitsEXT::error) {
			++errors;
		}

		if(info.flags & vk::DebugReportBitsEXT::warning) {
			++warnings;
		}

		if(info.flags & vk::DebugReportBitsEXT::performanceWarning) {
			++performanceWarnings;
		}

		return vpp::DebugCallback::call(info);
	}

	mutable unsigned int performanceWarnings {};
	mutable unsigned int warnings {};
	mutable unsigned int errors {};
};

constexpr auto pipeCacheFile = "testPipeCache.bin";

struct Globals {
	vpp::Instance instance;
	std::optional<CustomDebugCallback> debugCallback;
	std::optional<vpp::Device> device;

	vpp::RenderPass rp;
	vpp::ViewableImage attachment;
	vpp::Framebuffer fb;

	vpp::PipelineCache cache;
};

static Globals globals;

void initGlobals() {
	constexpr const char* iniExtensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};

	constexpr auto layer = "VK_LAYER_LUNARG_standard_validation";

	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = nullptr;
	instanceInfo.enabledLayerCount = 1;
	instanceInfo.ppEnabledLayerNames = &layer;
	instanceInfo.enabledExtensionCount = sizeof(iniExtensions) / sizeof(iniExtensions[0]);
	instanceInfo.ppEnabledExtensionNames = iniExtensions;

	globals.instance = {instanceInfo};
	globals.debugCallback.emplace(globals.instance);
	globals.device.emplace(globals.instance);
	auto& dev = *globals.device;

	dlg_info("Physical device info:\n\t{}",
		vpp::description(globals.device->vkPhysicalDevice(), "\n\t"));

	constexpr vk::Extent3D extent = {1920, 1080, 1};
	globals.attachment = {dev, *vpp::ViewableImageCreateInfo::color(dev,
		extent, vk::ImageUsageBits::colorAttachment)};

	vk::AttachmentDescription attachment;
	attachment.format = vk::Format::r8g8b8a8Unorm;
	attachment.samples = vk::SampleCountBits::e1;
	attachment.loadOp = vk::AttachmentLoadOp::clear;
	attachment.storeOp = vk::AttachmentStoreOp::store;
	attachment.stencilLoadOp = vk::AttachmentLoadOp::dontCare;
	attachment.stencilStoreOp = vk::AttachmentStoreOp::dontCare;
	attachment.initialLayout = vk::ImageLayout::undefined;
	attachment.finalLayout = vk::ImageLayout::presentSrcKHR;

	vk::AttachmentReference colorReference;
	colorReference.attachment = 0;
	colorReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::graphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	globals.rp = {dev, renderPassInfo};

	vk::FramebufferCreateInfo fbInfo;
	fbInfo.renderPass = globals.rp;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &globals.attachment.vkImageView();
	fbInfo.width = extent.width;
	fbInfo.height = extent.height;
	fbInfo.layers = 1u;

	globals.fb = {dev, fbInfo};
	globals.cache = {dev, pipeCacheFile};
}

int main() {
	initGlobals();

	auto ret = bugged::Testing::run();

	// try to save the cache
	dlg_assert(vpp::save(globals.cache, pipeCacheFile));

	// TODO: get this implicitly with RAII or at least a single "= {}"
	// explicitly destroy resources for errors
	globals.fb = {};
	globals.rp = {};
	globals.attachment = {};
	globals.cache = {};

	globals.device.reset();
	globals.debugCallback.reset();
	globals.instance = {};

	// add errors/warnings
	ret += globals.debugCallback->performanceWarnings;
	ret += globals.debugCallback->errors;
	ret += globals.debugCallback->warnings;

	return ret;
}
