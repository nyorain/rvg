// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/util.hpp>
#include <rvg/state.hpp>
#include <vpp/trackedDescriptor.hpp>
#include <vpp/vk.hpp>
#include <nytl/matOps.hpp>

namespace rvg {

// Transform
constexpr auto transformUboSize = sizeof(Mat4f);
Transform::Transform(Context& ctx, bool deviceLocal) :
	Transform(ctx, identity<4, float>(), deviceLocal) {
}

Transform::Transform(Context& ctx, const Mat4f& m, bool deviceLocal) :
		DeviceObject(ctx), matrix_(m) {

	auto usage = nytl::Flags {vk::BufferUsageBits::uniformBuffer};
	if(deviceLocal) {
		usage |= vk::BufferUsageBits::transferDst;
	}

	auto memBits = deviceLocal ?
		ctx.device().deviceMemoryTypes() :
		ctx.device().hostMemoryTypes();

	ubo_ = {ctx.bufferAllocator(), transformUboSize, usage, 4u, memBits};
	ds_ = {ctx.dsAllocator(), ctx.dsLayoutTransform()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

bool Transform::updateDevice() {
	dlg_assert(valid() && ubo_.size() && ds_);
	upload140(*this, ubo_, vpp::raw(matrix_));
	return false;
}

void Transform::update() {
	dlg_assert(valid() && ds_ && ubo_.size());
	context().registerUpdateDevice(this);
}

void Transform::bind(const DrawInstance& di) const {
	dlg_assert(valid() && ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), Context::transformBindSet, {ds_}, {});
}

// Scissor
constexpr auto scissorUboSize = sizeof(Vec2f) * 2;
Scissor::Scissor(Context& ctx, const Rect2f& r, bool deviceLocal)
	: DeviceObject(ctx), rect_(r) {

	auto usage = nytl::Flags {vk::BufferUsageBits::uniformBuffer};
	if(deviceLocal) {
		usage |= vk::BufferUsageBits::transferDst;
	}

	auto memBits = deviceLocal ?
		ctx.device().deviceMemoryTypes() :
		ctx.device().hostMemoryTypes();

	ubo_ = {ctx.bufferAllocator(), scissorUboSize, usage, 4u, memBits};
	ds_ = {ctx.dsAllocator(), ctx.dsLayoutScissor()};

	updateDevice();
	vpp::DescriptorSetUpdate update(ds_);
	update.uniform({{ubo_.buffer(), ubo_.offset(), ubo_.size()}});
}

void Scissor::update() {
	dlg_assert(valid() && ds_ && ubo_.size());
	context().registerUpdateDevice(this);
}

bool Scissor::updateDevice() {
	dlg_assert(ubo_.size() && ds_);
	upload140(*this, ubo_, vpp::raw(rect_.position),
		vpp::raw(rect_.size));
	return false;
}

void Scissor::bind(const DrawInstance& di) const {
	dlg_assert(ubo_.size() && ds_);
	vk::cmdBindDescriptorSets(di.cmdBuf, vk::PipelineBindPoint::graphics,
		di.context.pipeLayout(), Context::scissorBindSet, {ds_}, {});
}

} // namespace rvg
