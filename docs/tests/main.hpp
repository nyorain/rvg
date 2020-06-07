#define BUGGED_NO_MAIN
#include <bugged.hpp>

#include <rvg/context.hpp>
#include <rvg/font.hpp>
#include <vpp/vk.hpp>
#include <vpp/memory.hpp>
#include <vpp/device.hpp>
#include <vpp/submit.hpp>
#include <vpp/commandAllocator.hpp>
#include <vpp/queue.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/handles.hpp>
#include <vpp/imageOps.hpp>
#include <vpp/formats.hpp>
#include <vpp/image.hpp>
#include <vpp/debug.hpp>
#include <vpp/physicalDevice.hpp>
#include <dlg/dlg.hpp>
#include <memory>
#include <optional>

class CustomDebugMessenger : public vpp::DebugMessenger {
public:
	using vpp::DebugMessenger::DebugMessenger;

	void call(MsgSeverity severity,
			MsgTypeFlags type, const Data& data) noexcept override {

		if(severity == vk::DebugUtilsMessageSeverityBitsEXT::error) {
			++errors;
		}

		if(severity == vk::DebugUtilsMessageSeverityBitsEXT::warning) {
			++warnings;
		}

		vpp::DebugMessenger::call(severity, type, data);
	}

	mutable unsigned int warnings {};
	mutable unsigned int errors {};
};

constexpr auto pipeCacheFile = "testPipeCache.bin";

struct Globals {
	vpp::Instance instance;
	std::optional<CustomDebugMessenger> debugMessenger;
	std::optional<vpp::Device> device;

	vpp::RenderPass rp;
	vpp::ViewableImage attachment;
	vpp::Framebuffer fb;

	vpp::PipelineCache cache;
};

static Globals globals;

constexpr vk::Extent3D fbExtent = {512, 512, 1};
void initGlobals() {
	constexpr const char* iniExtensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};

	constexpr auto layer = "VK_LAYER_KHRONOS_validation";

	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = nullptr;
	instanceInfo.enabledLayerCount = 1;
	instanceInfo.ppEnabledLayerNames = &layer;
	instanceInfo.enabledExtensionCount = sizeof(iniExtensions) / sizeof(iniExtensions[0]);
	instanceInfo.ppEnabledExtensionNames = iniExtensions;

	globals.instance = {instanceInfo};
	globals.debugMessenger.emplace(globals.instance);
	globals.device.emplace(globals.instance);
	auto& dev = *globals.device;

	dlg_info("Physical device info:\n\t{}",
		vpp::description(globals.device->vkPhysicalDevice(), "\n\t"));

	auto usage = vk::ImageUsageBits::colorAttachment |
		vk::ImageUsageBits::transferSrc;
	auto vci = vpp::ViewableImageCreateInfo(vk::Format::r8g8b8a8Unorm,
		vk::ImageAspectBits::color, {fbExtent.width, fbExtent.height}, usage);
	dlg_assert(vpp::supported(dev, vci.img));
	globals.attachment = {dev.devMemAllocator(), vci};

	vk::AttachmentDescription attachment;
	attachment.format = vk::Format::r8g8b8a8Unorm;
	attachment.samples = vk::SampleCountBits::e1;
	attachment.loadOp = vk::AttachmentLoadOp::clear;
	attachment.storeOp = vk::AttachmentStoreOp::store;
	attachment.stencilLoadOp = vk::AttachmentLoadOp::dontCare;
	attachment.stencilStoreOp = vk::AttachmentStoreOp::dontCare;
	attachment.initialLayout = vk::ImageLayout::undefined;
	attachment.finalLayout = vk::ImageLayout::transferSrcOptimal;

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
	fbInfo.width = fbExtent.width;
	fbInfo.height = fbExtent.height;
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
	globals.debugMessenger.reset();
	globals.instance = {};

	// add errors/warnings
	ret += globals.debugMessenger->errors;
	ret += globals.debugMessenger->warnings;

	return ret;
}

std::unique_ptr<rvg::Context> createContext() {
	rvg::ContextSettings settings;
	settings.renderPass = globals.rp;
	settings.subpass = 0u;
	settings.pipelineCache = globals.cache;
	return std::make_unique<rvg::Context>(*globals.device, settings);
}

template<typename F1, typename F2 = bool>
vpp::CommandBuffer record(rvg::Context& ctx, F1&& renderer, F2&& after = {}) {
	const auto clearValue = vk::ClearValue {{0.f, 0.f, 0.f, 1.f }};
	auto width = fbExtent.width;
	auto height = fbExtent.height;

	auto& dev = ctx.device();
	auto qf = dev.queueSubmitter().queue().family();
	auto cmdBuf = dev.commandAllocator().get(qf);

	vk::beginCommandBuffer(cmdBuf, {});
	vk::cmdBeginRenderPass(cmdBuf, {
		globals.rp,
		globals.fb,
		{0u, 0u, width, height},
		1,
		&clearValue
	}, {});

	vk::Viewport vp {0.f, 0.f, (float) width, (float) height, 0.f, 1.f};
	vk::cmdSetViewport(cmdBuf, 0, 1, vp);
	vk::cmdSetScissor(cmdBuf, 0, 1, {0, 0, width, height});

	ctx.bindDefaults(cmdBuf);
	renderer(cmdBuf);

	vk::cmdEndRenderPass(cmdBuf);

	if constexpr(!std::is_same_v<F2, bool>) {
		after(cmdBuf);
	}

	vk::endCommandBuffer(cmdBuf);
	return cmdBuf;
}

void renderSubmit(rvg::Context& ctx, vk::CommandBuffer cmdBuf) {
	auto semaphore = ctx.stageUpload();

	vk::SubmitInfo submission;
	submission.commandBufferCount = 1u;
	submission.pCommandBuffers = &cmdBuf;

	if(semaphore) {
		static auto stage = nytl::Flags {vk::PipelineStageBits::allGraphics};
		submission.pWaitSemaphores = &semaphore;
		submission.pWaitDstStageMask = &stage;
		submission.waitSemaphoreCount = 1u;
	}

	auto& qs = ctx.device().queueSubmitter();
	auto id = qs.add(submission);
	qs.wait(id);
}

vpp::SubBuffer readImage(vk::CommandBuffer cb) {
	auto& a = globals.attachment;
	auto range = vpp::retrieveStaging(cb, a.image(),
		vk::Format::r8g8b8a8Unorm,
		vk::ImageLayout::transferSrcOptimal, fbExtent,
		{vk::ImageAspectBits::color, 0,  0});

	return range;
}
