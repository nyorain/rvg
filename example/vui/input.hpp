#pragma once

#include "fwd.hpp"
#include <nytl/vec.hpp>

namespace vui {

enum class MouseButton {
	left = 2,
	right,
	middle,
	custom1,
	custom2
};

enum class Key {
	escape = 1,
	backspace = 14,
	enter = 28,
	up = 103,
	left = 105,
	right = 106,
	del = 111,
	down = 108,
};

// events
struct MouseMoveEvent {
	Vec2f position;
};

struct MouseButtonEvent {
	bool pressed;
	MouseButton button;
	Vec2f position;
};

struct MouseWheelEvent {
	float distance;
};

struct KeyEvent {
	Key key;
	bool pressed;
};

struct TextInputEvent {
	const char* utf8;
};

} // namespace vui
