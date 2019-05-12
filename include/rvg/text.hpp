// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>
#include <rvg/font.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/vec.hpp>
#include <nytl/matOps.hpp>
#include <nytl/rect.hpp>

#include <vpp/descriptor.hpp>
#include <vpp/sharedBuffer.hpp>

#include <string>
#include <string_view>

namespace rvg {

/// Represents text to be drawn.
/// Also offers some utility for bounds querying.
class Text : public DeviceObject {
public:
	Text() = default;
	Text(Context&, Vec2f pos, std::string text, const Font&, float height);

	Text(Text&& rhs) noexcept;
	Text& operator=(Text&& rhs) noexcept;
	~Text();

	/// Draws this text with the bound draw resources (transform,
	/// scissor, paint).
	void draw(vk::CommandBuffer) const;

	auto change() { return StateChange {*this, state_}; }
	bool disable(bool);
	bool disabled() const { return disable_; }

	/// Changes the device local state for this text.
	/// If unequal the previous value, will always recreate the buffer and
	/// trigger a rerecord.
	void deviceLocal(bool set);
	bool deviceLocal() const { return deviceLocal_; }

	/// Computes which char index lies at the given relative x.
	/// Returns the index of the char at the given x, or the index of
	/// the next one if there isn't any. Returns text.length() if x is
	/// after all chars.
	/// Must not be called during a state change.
	unsigned charAt(float x) const;

	/// Returns the (local) bounds of the full text
	Rect2f bounds() const;

	/// Returns the bounds of the ith char in local coordinates.
	Rect2f ithBounds(unsigned n) const;

	const auto& font() const { return state_.font; }
	const auto& text() const { return state_.text; }
	const auto& position() const { return state_.position; }
	float height() const { return state_.height; }
	float width() const;

	void update();
	bool updateDevice();

protected:
	struct State {
		std::string text {};
		Font font {}; // must not be set to invalid font
		Vec2f position {}; // baseline position of first character
		float height {}; // height to use
		// pre-transform
		nytl::Mat3f transform = nytl::identity<3, float>();
	} state_;

	bool disable_ {};
	bool deviceLocal_ {false};

	std::vector<Vec2f> posCache_;
	std::vector<Vec2f> uvCache_;
	vpp::SubBuffer posBuf_;
	vpp::SubBuffer uvBuf_;
	FontAtlas* oldAtlas_ {};
};

} // namespace rvg
