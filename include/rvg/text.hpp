// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/vec.hpp>
#include <nytl/rect.hpp>

#include <vpp/descriptor.hpp>
#include <vpp/sharedBuffer.hpp>

#include <string>
#include <string_view>

namespace rvg {

/// Represents text to be drawn.
class Text : public DeviceObject {
public:
	Text() = default;
	Text(Context&, std::u32string text, const Font&, Vec2f pos);
	Text(Context&, std::string_view text, const Font&, Vec2f pos);

	/// Draws this text with the bound draw resources (transform,
	/// scissor, paint).
	void draw(const DrawInstance&) const;

	auto change() { return StateChange {*this, state_}; }
	bool disable(bool);
	bool disabled() const { return disable_; }

	/// Computes which char index lies at the given relative x.
	/// Returns the index of the char at the given x, or the index of
	/// the next one if there isn't any. Returns text.length() if x is
	/// after all chars.
	unsigned charAt(float x) const;

	/// Returns the bounds of the ith char in local coordinates.
	/// For a char that has no x-size (e.g. space), returns xadvance
	/// as x size.
	Rect2f ithBounds(unsigned n) const;

	const auto& font() const { return state_.font; }
	const auto& utf32() const { return state_.utf32; }
	const auto& position() const { return state_.position; }
	auto utf8() const { return state_.utf8(); }
	float width() const;

	void update();
	bool updateDevice();

protected:
	struct State {
		std::u32string utf32 {};
		const Font* font {};
		Vec2f position {};

		void utf8(std::string_view);
		std::string utf8() const;
	} state_;

	bool disable_ {};
	std::vector<Vec2f> posCache_;
	std::vector<Vec2f> uvCache_;
	vpp::BufferRange buf_;
	const Font* oldFont_ {};
};

} // namespace rvg
