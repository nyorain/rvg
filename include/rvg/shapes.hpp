// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/polygon.hpp>
#include <rvg/stateChange.hpp>

#include <nytl/vec.hpp>
#include <nytl/rect.hpp>

#include <vpp/descriptor.hpp>
#include <vpp/sharedBuffer.hpp>

namespace rvg {

/// Shape manually specified by its outlining points.
/// Can only fill convex shapes correctly.
class Shape {
public:
	Shape() = default;
	Shape(Context& ctx) : polygon_(ctx) {}
	Shape(Context&, std::vector<Vec2f> points, const DrawMode&);

	auto change() { return StateChange {*this, state_}; }

	void fill(vk::CommandBuffer cb) const { polygon_.fill(cb); }
	void stroke(vk::CommandBuffer cb) const { polygon_.stroke(cb); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& points() const { return state_.points; }
	const auto& drawMode() const { return state_.drawMode; }
	const auto& polygon() const { return polygon_; }
	void update();

protected:
	struct State {
		std::vector<Vec2f> points;
		DrawMode drawMode {};
	} state_;

	Polygon polygon_;
};

/// Rectangle shape that can be filled or stroked.
class RectShape {
public:
	RectShape() = default;
	RectShape(Context& ctx) : polygon_(ctx) {}
	RectShape(Context&, Vec2f pos, Vec2f size, DrawMode,
		std::array<float, 4> round = {0.f, 0.f, 0.f, 0.f});

	auto change() { return StateChange {*this, state_}; }

	void fill(vk::CommandBuffer cb) const { polygon_.fill(cb); }
	void stroke(vk::CommandBuffer cb) const { polygon_.stroke(cb); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& size() const { return state_.size; }
	const auto& position() const { return state_.position; }
	const auto& drawMode() const { return state_.drawMode; }
	const auto& rounding() const { return state_.rounding; }
	const auto& polygon() const { return polygon_; }
	Rect2f bounds() const { return {position(), size()}; }

	void update();

protected:
	struct {
		Vec2f position {};
		Vec2f size {};
		DrawMode drawMode {};
		std::array<float, 4> rounding {};

		/// TODO(v0.2): does not work correctly atm
		nytl::Mat3f transform = nytl::identity<3, float>();
	} state_;

	Polygon polygon_;
};

/// Circular shape that can be filled or stroked.
class CircleShape {
public:
	/// When this is passed, will automatically choose the number
	/// of points in the circle polygon.
	static constexpr unsigned defaultPointCount = 0u;

public:
	CircleShape() = default;
	CircleShape(Context& ctx) : polygon_(ctx) {}
	CircleShape(Context&, Vec2f center, Vec2f radius, const DrawMode&,
		unsigned points = defaultPointCount, float startAngle = 0.f);
	CircleShape(Context&, Vec2f center, float radius, const DrawMode&,
		unsigned points = defaultPointCount, float startAngle = 0.f);

	auto change() { return StateChange {*this, state_}; }

	void fill(vk::CommandBuffer cb) const { polygon_.fill(cb); }
	void stroke(vk::CommandBuffer cb) const { polygon_.stroke(cb); }

	auto& context() const { return polygon_.context(); }
	void disable(bool d, DrawType t = DrawType::strokeFill);
	bool disabled(DrawType t = DrawType::strokeFill) const;

	const auto& center() const { return state_.center; }
	const auto& radius() const { return state_.radius; }
	const auto& drawMode() const { return state_.drawMode; }
	const auto& pointCount() const { return state_.pointCount; }
	const auto& startAngle() const { return state_.startAngle; }
	const auto& polygon() const { return polygon_; }
	void update();

protected:
	struct {
		Vec2f center {};
		Vec2f radius {};
		DrawMode drawMode {};
		unsigned pointCount {defaultPointCount};
		float startAngle {0.f};

		/// TODO(v0.2): does not work correctly atm
		nytl::Mat3f transform = nytl::identity<3, float>();
	} state_;

	Polygon polygon_;
};

} // namespace rvg
