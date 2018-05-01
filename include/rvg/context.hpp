// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/state.hpp>
#include <rvg/paint.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/commandBuffer.hpp>
#include <vpp/image.hpp>
#include <vpp/sync.hpp>

#include <variant>
#include <unordered_set>

namespace rvg {

/// Represents a render pass drawing (recording) instance using a context.
struct DrawInstance {
	Context& context;
	vk::CommandBuffer cmdBuf;
};

struct ContextSettings {
	/// The renderpass and subpass in which it will be used.
	/// Must be specified for pipeline creation.
	vk::RenderPass renderPass;
	unsigned subpass;

	/// Whether the device has the clipDistance feature enabled.
	/// Can increase scissor performance significantly.
	bool clipDistanceEnable {false};

	/// Whether to support antialiasing. Still has to be enabled for
	/// every shape (DrawMode). Using it can introduce a significant
	/// overhead while just enabling it here should only have a minor
	/// impact. If not enabled here, the DrawMode stroke/fill aa settings
	/// always have to be set to false.
	bool antiAliasing {true};

	/// The pipeline cache to use.
	/// Will use no cache if left empty.
	vk::PipelineCache pipelineCache {};

	// TODO
	/// The QueueSubmitter to submit any upload work to.
	/// Should be associated with the queue for rendering since
	/// otherwise Context::waitSemaphore cannot be used for rendering.
	/// In this case, one could still synchronize using fences.
	// vpp::QueueSubmitter& submitter;
};

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes.
class Context {
public:
	// TODO: name duplication to rvg::DeviceObject probably
	// bad idea for readability.
	using DeviceObject = std::variant<
		Polygon*,
		Text*,
		Paint*,
		Texture*,
		Transform*,
		Scissor*>;

	static constexpr auto transformBindSet = 0u;
	static constexpr auto paintBindSet = 1u;
	static constexpr auto fontBindSet = 2u;
	static constexpr auto scissorBindSet = 3u;
	static constexpr auto aaStrokeBindSet = 4u;

	static constexpr auto fringe() { return 1.5f; }

public:
	Context(vpp::Device&, const ContextSettings&);

	/// Must be called once per frame when there is no command buffer
	/// executing that references objects associated with this context.
	/// Will update device objects (like buffers).
	/// Returns whether a rerecord is needed. Submitting a previously
	/// recorded command buffer referencing objects associated with this
	/// Context when this returns true results in undefined behaviour.
	bool updateDevice();

	/// Starts a command buffer recording.
	/// The DrawInstance can then be used to draw objects associated
	/// with this context.
	DrawInstance record(vk::CommandBuffer);

	/// Queues all pending upload operations and returns the semaphore to wait
	/// upon for the next render call.
	/// Must be waited upon with the indirectDraw stage bit.
	/// Used to make sure that new data was uploaded to deviceLocal
	/// vulkan resources.
	/// Must not be called again until rendering this frame completes.
	vk::Semaphore stageUpload();

	void rerecord() { rerecord_ = true; }

	const auto& device() const { return device_; };
	const auto& pipeLayout() const { return pipeLayout_; }
	const auto& fanPipe() const { return fanPipe_; }
	const auto& stripPipe() const { return stripPipe_; }

	const auto& dsLayoutTransform() const { return dsLayoutTransform_; }
	const auto& dsLayoutScissor() const { return dsLayoutScissor_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutFontAtlas() const { return dsLayoutFontAtlas_; }
	const auto& dsLayoutStrokeAA() const { return dsLayoutStrokeAA_; }

	vpp::DescriptorAllocator& dsAllocator() const;
	vpp::BufferAllocator& bufferAllocator() const;

	const auto& emptyImage() const { return emptyImage_; };

	const auto& dummyTex() const { return dummyTex_; };
	const auto& pointColorPaint() const { return pointColorPaint_; }
	const auto& identityTransform() const { return identityTransform_; }
	const auto& defaultScissor() const { return defaultScissor_; }
	const auto& defaultStrokeAA() const { return defaultStrokeAA_; }

	const auto& settings() const { return settings_; }
	bool antiAliasing() const { return settings().antiAliasing; }

	// internal DeviceObject communication
	vpp::CommandBuffer uploadCmdBuf();
	void addCommandBuffer(DeviceObject, vpp::CommandBuffer&&);
	void addStage(vpp::SubBuffer&& buf);

	void registerUpdateDevice(DeviceObject);
	bool deviceObjectDestroyed(::rvg::DeviceObject&) noexcept;
	void deviceObjectMoved(::rvg::DeviceObject&, ::rvg::DeviceObject&) noexcept;

private:
	// Per-frame objects mainly used to efficiently upload data
	struct Temporaries {
		std::vector<std::pair<DeviceObject, vpp::CommandBuffer>> cmdBufs;
		std::vector<vpp::SubBuffer> stages;
	};

	// NOTE: order here is rather important since some of them depend
	// on each other. Don't change unless you know what you
	// are doing (so probably: don't change!).
	const vpp::Device& device_;
	const ContextSettings settings_;
	std::unordered_set<DeviceObject> updateDevice_;

	Temporaries currentFrame_;
	Temporaries oldFrame_;

	vpp::Pipeline fanPipe_;
	vpp::Pipeline stripPipe_;
	vpp::PipelineLayout pipeLayout_;

	vpp::TrDsLayout dsLayoutTransform_;
	vpp::TrDsLayout dsLayoutScissor_;
	vpp::TrDsLayout dsLayoutPaint_;
	vpp::TrDsLayout dsLayoutFontAtlas_;
	vpp::TrDsLayout dsLayoutStrokeAA_;

	vpp::Sampler fontSampler_;
	vpp::Sampler texSampler_;

	Texture emptyImage_;
	vpp::TrDs dummyTex_;

	Scissor defaultScissor_;
	Transform identityTransform_;
	Paint pointColorPaint_;

	vpp::SubBuffer defaultStrokeAABuf_;
	vpp::TrDs defaultStrokeAA_;

	bool rerecord_ {};

	vpp::Semaphore uploadSemaphore_;
	vpp::CommandBuffer uploadCmdBuf_;
};

} // namespace vgv
