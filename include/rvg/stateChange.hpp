// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <exception>

namespace rvg {
namespace detail {
	void outputException(const std::exception& err);
} // namespace detail

/// Small RAII wrapper around changing a DeviceObjects contents.
/// As long as the object is alive, the state can be changed. When
/// it gets destructed, the update function of the original object will
/// be called, i.e. the changed state will be applied.
///
/// For example, using a RectShape:
/// ```cpp
/// RectShape rect = {...};
/// auto rc = rect.change();
/// rc->position += Vec {10.f, 10.f};
/// rc->size.y = 0.f;
/// rc->rounding[0] = 2.f;
/// ```
/// When 'rc' goes out of scope in the above example, the rect shape
/// is updated (i.e. the fill/stroke/aa points baked).
/// Doing so only once for all those changes instead of again
/// after every single change can increase performance and avoids
/// redundant work.
/// If you only want to change one parameter, you can still
/// use it inline like this: ```rect.change()->position.x = 0.f```.
template<typename T, typename S>
class StateChange {
public:
	T& object;
	S& state;

public:
	~StateChange() {
		// We don't throw here since throwing from destructor
		// is bad idea.
		try {
			object.update();
		} catch(const std::exception& err) {
			detail::outputException(err);
		}
	}

	S* operator->() { return &state; }
	S& operator*() { return state; }
};

template<typename T, typename S>
StateChange(T&, S&) -> StateChange<T, S>;

} // namespace rvg

