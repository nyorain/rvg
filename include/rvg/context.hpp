// Copyright (c) 2019 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/state.hpp>
#include <rvg/paint.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>
#include <vpp/handles.hpp>
#include <nytl/nonCopyable.hpp>

#include <variant>
#include <unordered_set>

namespace rvg {

/// Control various aspects of a context.
/// You have to set those members that have no default value to valid
/// values.
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

	/// The multisample bits to use for the pipelines.
	vk::SampleCountBits samples {};
};

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes. There is usually no need for multiple Contexts
/// for a single device.
class Context : public nytl::NonMovable {
public:
	/// All resources with data on the device that might
	/// register themselves for update device calls.
	using DevRes = std::variant<
		Polygon*,
		Text*,
		Paint*,
		Texture*,
		Transform*,
		Scissor*,
		FontAtlas*>;

	/// Descriptor set bindings.
	static constexpr auto transformBindSet = 0u;
	static constexpr auto paintBindSet = 1u;
	static constexpr auto fontBindSet = 2u;
	static constexpr auto scissorBindSet = 3u;
	static constexpr auto aaStrokeBindSet = 4u;

	/// Specifies the thickness of the anti aliasing area.
	/// A greater fringe may result in smoother but also
	/// more blurry edges.
	static constexpr auto fringe() { return 1.5f; }

public:
	/// Creates a new context for given device and settings.
	/// The device must remain valid for the lifetime of this context.
	/// You should generally avoid to create multiple contexts for one
	/// device since a context creates and manages expensive resources
	/// like pipelines.
	Context(vpp::Device&, const ContextSettings&);

	/// Binds default state on the given command buffer (which must be
	/// in recording state). Can then be used for rendering without
	/// the need for binding a custom scissor or transform.
	/// Does NOT bind a paint. [TODO(v0.2)]
	void bindDefaults(vk::CommandBuffer);

	/// Calls stageUpload and updateDevice.
	/// Returns whether a rerecord is needed (from updateDevice) and
	/// the semaphore (or a null handle) that rendering must
	/// wait upon (with allGraphics stage bit).
	/// - submit: Will be passed to stageUpload.
	std::pair<bool, vk::Semaphore> upload(bool submit = false);

	/// Queues all pending upload operations and returns the semaphore to wait
	/// upon for the next render call.
	/// Must be waited upon with the indirectDraw stage bit.
	/// Used to make sure that new data was uploaded to deviceLocal
	/// vulkan resources.
	/// Must not be called again until rendering this frame completes.
	/// - submit: Whether or not to already submit the work (if there is
	///   any). It will be submitted to the device's default queueSubmitter.
	///   If this is false, the submission must later on be done manually,
	///   the work will only be queued (i.e. added) to the device's
	///   queueSubmitter.
	vk::Semaphore stageUpload(bool submit = false);

	/// Must be called once per frame when there is no command buffer
	/// executing that references objects associated with this context.
	/// Will update device objects (like buffers).
	/// Returns whether a rerecord is needed. Submitting a previously
	/// recorded command buffer referencing objects associated with this
	/// Context when this returns true results in undefined behaviour.
	bool updateDevice();

	/// Signal that a rerecord is needed.
	void rerecord() { rerecord_ = true; }


	// internal resources, mainly used by other rvg classes for rendering
	const auto& device() const { return device_; };
	const auto& pipeLayout() const { return pipeLayout_; }
	const auto& fanPipe() const { return fanPipe_; }
	const auto& stripPipe() const { return stripPipe_; }

	const auto& dsLayoutTransform() const { return dsLayoutTransform_; }
	const auto& dsLayoutScissor() const { return dsLayoutScissor_; }
	const auto& dsLayoutPaint() const { return dsLayoutPaint_; }
	const auto& dsLayoutFontAtlas() const { return dsLayoutFontAtlas_; }
	const auto& dsLayoutStrokeAA() const { return dsLayoutStrokeAA_; }

	// TODO: allow to assign custom allocators per settings
	vpp::DescriptorAllocator& dsAllocator() const;
	vpp::BufferAllocator& bufferAllocator() const;
	vpp::DeviceMemoryAllocator& devMemAllocator() const;

	const auto& emptyImage() const { return emptyImage_; };

	const auto& dummyTex() const { return dummyTex_; };
	const auto& pointColorPaint() const { return pointColorPaint_; }
	const auto& identityTransform() const { return identityTransform_; }
	const auto& defaultScissor() const { return defaultScissor_; }
	const auto& defaultStrokeAA() const { return defaultStrokeAA_; }
	const auto& defaultAtlas() const { return *defaultAtlas_; }
	auto& defaultAtlas() { return *defaultAtlas_; }

	const auto& settings() const { return settings_; }
	bool antiAliasing() const { return settings().antiAliasing; }

	// internal DeviceObject communication
	vpp::CommandBuffer uploadCmdBuf();
	void addCommandBuffer(DevRes, vpp::CommandBuffer&&);
	void addStage(vpp::SubBuffer&& buf);

	void registerUpdateDevice(DevRes);
	bool deviceObjectDestroyed(::rvg::DeviceObject&) noexcept;
	void deviceObjectMoved(::rvg::DeviceObject&, ::rvg::DeviceObject&) noexcept;

private:
	// Per-frame objects mainly used to efficiently upload data
	struct Temporaries {
		std::vector<std::pair<DevRes, vpp::CommandBuffer>> cmdBufs;
		std::vector<vpp::SubBuffer> stages;
	};

	// NOTE: order here is rather important since some of them depend
	// on each other. Don't change unless you know what you
	// are doing (so probably: don't change period).
	const vpp::Device& device_;
	const ContextSettings settings_;
	std::unordered_set<DevRes> updateDevice_;

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

	vpp::Sampler texSampler_;

	Texture emptyImage_;
	vpp::TrDs dummyTex_;

	Scissor defaultScissor_;
	Transform identityTransform_;
	Paint pointColorPaint_;
	std::unique_ptr<FontAtlas> defaultAtlas_;

	vpp::SubBuffer defaultStrokeAABuf_;
	vpp::TrDs defaultStrokeAA_;

	bool rerecord_ {};

	vpp::Semaphore uploadSemaphore_;
	vpp::CommandBuffer uploadCmdBuf_;
};

} // namespace rvg
