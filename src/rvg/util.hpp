// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/context.hpp>
#include <dlg/dlg.hpp>
#include <vpp/sharedBuffer.hpp>
#include <vpp/bufferOps.hpp>
#include <vpp/vk.hpp>
#include <dlg/dlg.hpp>

#include <cstdlib>
#include <cstring>

namespace rvg {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

struct Uploader {
	template<typename T>
	void write(nytl::Span<T> span) {
		auto bytes = nytl::as_bytes(span);
		dlg_assert(bytes.size() <= data.size());
		std::memcpy(data.data(), bytes.data(), bytes.size());
		data = data.last(data.size() - bytes.size());
	}

	template<typename T>
	void write(const T& obj) {
		write(nytl::Span<const T>(&obj, 1));
	}

	nytl::Span<std::byte> data;
};

template<typename O, typename... Args>
std::size_t writeBuffer(O& dobj, vpp::BufferSpan buf, const Args&... args) {
	dlg_assert(buf.valid());

	if(!buf.buffer().mappable()) {
		auto& ctx = dobj.context();
		auto cb = ctx.uploadCmdBuf();
		auto stage = vpp::SubBuffer(ctx.bufferAllocator(),
			buf.size(), vk::BufferUsageBits::transferSrc, 4u);
		auto size = writeBuffer(dobj, stage, args...);

		vk::BufferCopy copy;
		copy.srcOffset = stage.offset();
		copy.dstOffset = buf.offset();
		copy.size = size;
		vk::cmdCopyBuffer(cb, stage.buffer(), buf.buffer(), {{copy}});

		dobj.context().addStage(std::move(stage));
		dobj.context().addCommandBuffer(&dobj, std::move(cb));
		return size;
	}

	vpp::MemoryMapView map;
	map = buf.memoryMap();
	Uploader uploader;
	uploader.data = map.span();

	(uploader.write(args), ...);
	return uploader.data.data() - map.ptr();
}

inline Vec2f multPos(const nytl::Mat3f& transform, nytl::Vec2f pos) {
	auto p3 = transform * Vec3f{pos.x, pos.y, 1.f};
	return (1.f / p3.z) * Vec2f{p3.x, p3.y};
}

inline Vec2f multDir(const nytl::Mat3f& transform, nytl::Vec2f pos) {
	auto p3 = transform * Vec3f{pos.x, pos.y, 0.f};
	return Vec2f{p3.x, p3.y};
}

// Returns by how much the given transform matrix scales an axis on average.
inline float scale(const nytl::Mat3f& t) {
	// square root of determinant should work since the determinant
	// describes by how much this transform would multiply an area
	return std::sqrt(t[0][0] * t[1][1] - t[0][1] * t[1][0]);
}

} // namespace rvg
