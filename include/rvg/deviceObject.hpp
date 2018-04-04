// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>

namespace rvg {

/// Simple base class for objects that register themself for
/// updateDevice callbacks at their associated Context.
class DeviceObject {
public:
	DeviceObject() = default;
	DeviceObject(Context& ctx) : context_(&ctx) {}
	~DeviceObject();

	DeviceObject(DeviceObject&& rhs) noexcept;
	DeviceObject& operator=(DeviceObject&& rhs) noexcept;

	Context& context() const { return *context_; }
	bool valid() const { return context_; }
	void update();

private:
	Context* context_ {};
};

} // namespace rvg
