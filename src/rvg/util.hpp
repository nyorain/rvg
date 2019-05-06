// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/context.hpp>
#include <vpp/bufferOps.hpp>
#include <dlg/dlg.hpp>

#include <cstdlib>
#include <cstring>

namespace rvg {

template<typename T>
void write(std::byte*& ptr, T&& data) {
	std::memcpy(ptr, &data, sizeof(data));
	ptr += sizeof(data);
}

template<typename O, typename... Args>
void upload140(O& dobj, const vpp::BufferSpan& buf, const Args&... args) {
	dlg_assert(buf.valid());
	if(buf.buffer().mappable()) {
		vpp::writeMap140(buf, args...);
	} else {
		auto cmdBuf = dobj.context().uploadCmdBuf();
		dobj.context().addStage(vpp::writeStaging(cmdBuf, buf,
			vpp::BufferLayout::std140, args...));
		dobj.context().addCommandBuffer(&dobj, std::move(cmdBuf));
	}
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
