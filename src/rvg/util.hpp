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

} // namespace rvg
