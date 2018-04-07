// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/state.hpp>
#include <rvg/paint.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/pipeline.hpp>
#include <vpp/image.hpp>

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
	unsigned subpass {0};

	/// Whether the device has the clipDistance feature enabled.
	/// Can increase scissor performance significantly.
	bool clipDistanceEnable {false};

	/// Whether to support antialiasing. Still has to be enabled for
	/// every shape (DrawMode). Using it can introduce a significant
	/// overhead while just enabling it here should only have a minor
	/// impact. If not enabled here, the DrawMode stroke/fill aa settings
	/// always have to be set to false.
	bool antiAliasing {true};
};

/// Drawing context. Manages all pipelines and layouts needed to
/// draw any shapes.
class Context {
public:
	using DeviceObject = std::variant<
		Polygon*,
		Text*,
		Paint*,
		Transform*,
		Scissor*>;

	static constexpr auto transformBindSet = 0u;
	static constexpr auto paintBindSet = 1u;
	static constexpr auto fontBindSet = 2u;
	static constexpr auto scissorBindSet = 3u;
	static constexpr auto aaStrokeBindSet = 4u;

	static constexpr auto fringe() { return 2.f; }

public:
	Context(vpp::Device&, const ContextSettings&);

	bool updateDevice();
	DrawInstance record(vk::CommandBuffer);

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

	const auto& emptyImage() const { return emptyImage_; };

	const auto& dummyTex() const { return dummyTex_; };
	const auto& pointColorPaint() const { return pointColorPaint_; }
	const auto& identityTransform() const { return identityTransform_; }
	const auto& defaultScissor() const { return defaultScissor_; }
	const auto& defaultStrokeAA() const { return defaultStrokeAA_; }

	const auto& settings() const { return settings_; }
	bool antiAliasing() const { return settings().antiAliasing; }

	void registerUpdateDevice(DeviceObject);
	bool deviceObjectDestroyed(::rvg::DeviceObject&) noexcept;
	void deviceObjectMoved(::rvg::DeviceObject&, ::rvg::DeviceObject&) noexcept;

private:
	const vpp::Device& device_;

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

	vpp::ViewableImage emptyImage_;
	vpp::DescriptorSet dummyTex_;

	Scissor defaultScissor_;
	Transform identityTransform_;
	Paint pointColorPaint_;

	vpp::SubBuffer defaultStrokeAABuf_;
	vpp::DescriptorSet defaultStrokeAA_;

	std::unordered_set<DeviceObject> updateDevice_;
	bool rerecord_ {};

	const ContextSettings settings_;
};

} // namespace vgv
