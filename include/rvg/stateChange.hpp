// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>

namespace rvg {

/// Small RAII wrapper around changing a DeviceObjects contents.
/// As long as the object is alived, the state can be changed. When
/// it gets destructed, the update function of the original object will
/// be called, i.e. the changed state will be applied.
template<typename T, typename S>
class StateChange {
public:
	T& object;
	S& state;

public:
	// TODO: catch all exception from object.update()?
	// might be problematic otherwise since this might throw from
	// destructor. Or only catch exceptions if destructor is called
	// due to unrolling (nytl/scope like)?
	// TODO: somehow assure that there is only ever one StateChange for one
	// object at a time? Probably needs to be done in the objects.
	// Is it even needed? not sure.
	~StateChange() {
		object.update();
	}

	S* operator->() { return &state; }
	S& operator*() { return state; }
};

template<typename T, typename S>
StateChange(T&, S&) -> StateChange<T, S>;

} // namespace rvg

