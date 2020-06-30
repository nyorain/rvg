// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/rect.hpp>
#include <nytl/mat.hpp>

#include <vpp/trackedDescriptor.hpp>
#include <vpp/sharedBuffer.hpp>

namespace rvg {

/// Matrix-based transform used to specify how shape coordinates
/// are mapped to the output.
/// You can specify in the constructor if the transform should be
/// stored on deviceLocal device memory - which is usually faster to use
/// but more expensive to update - or otherwise on hostVisibile memory which
/// should be used for frequently updating transforms.
class Transform : public DeviceObject {
public:
	Transform() = default;
	Transform(Context& ctx, bool deviceLocal = true); // uses identity matrix
	Transform(Context& ctx, const Mat4f&, bool deviceLocal = true);

	/// Binds the transform in the given command buffer.
	/// All following stroke/fill calls are affected by this transform object,
	/// until another transform is bound.
	void bind(vk::CommandBuffer) const;

	auto change() { return StateChange {*this, matrix_}; }
	auto& matrix() const { return matrix_; }
	void matrix(const Mat4f& matrix) { *change() = matrix; }

	auto& ubo() const { return ubo_; }
	auto& ds() const { return ds_; }

	void update();
	bool updateDevice();

protected:
	Mat4f matrix_;
	vpp::SubBuffer ubo_;
	vpp::TrDs ds_;
};

/// Limits the area in which can be drawn.
/// Scissor is applied before the transformation.
/// You can specify in the constructor if the scissor should be
/// stored on deviceLocal device memory - which is usually faster to use
/// but more expensive to update - or otherwise on hostVisibile memory which
/// should be used for frequently updating transforms.
class Scissor : public DeviceObject {
public:
	static constexpr Rect2f reset = {{-1e6, -1e6}, {2e6, 2e6}};

public:
	Scissor() = default;
	Scissor(Context&, const Rect2f& = reset, bool deviceLocal = true);

	/// Binds the scissor in the given DrawInstance.
	/// All following stroke/fill calls are affected by this scissor object,
	/// until another scissor is bound.
	void bind(vk::CommandBuffer) const;

	auto change() { return StateChange {*this, rect_}; }
	void rect(const Rect2f& rect) { *change() = rect; }
	auto& rect() const { return rect_; }

	auto& ubo() const { return ubo_; }
	auto& ds() const { return ds_; }
	void update();
	bool updateDevice();

protected:
	Rect2f rect_ = reset;
	vpp::SubBuffer ubo_;
	vpp::TrDs ds_;
};

} // namespace rvg
