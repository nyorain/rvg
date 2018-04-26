// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/shapes.hpp>
#include <rvg/context.hpp>
#include <katachi/path.hpp>
#include <katachi/curves.hpp>
#include <dlg/dlg.hpp>

namespace rvg {

// Shape
Shape::Shape(Context& ctx, std::vector<Vec2f> p, const DrawMode& d) :
		state_{std::move(p), std::move(d)}, polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

void Shape::update() {
	polygon_.update(state_.points, state_.drawMode);
}

void Shape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool Shape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

// Rect
RectShape::RectShape(Context& ctx, Vec2f p, Vec2f s,
	DrawMode d, std::array<float, 4> round) :
		state_{p, s, std::move(d), round}, polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

void RectShape::update() {
	if(state_.rounding == std::array<float, 4>{}) {
		auto points = {
			state_.position,
			state_.position + Vec {state_.size.x, 0.f},
			state_.position + state_.size,
			state_.position + Vec {0.f, state_.size.y},
			state_.position
		};
		polygon_.update(points, state_.drawMode);
	} else {
		constexpr auto steps = 12u;
		std::vector<Vec2f> points;

		auto& position = state_.position;
		auto& size = state_.size;
		auto& rounding = state_.rounding;

		// topRight
		if(rounding[0] != 0.f) {
			dlg_assert(rounding[0] > 0.f);
			points.push_back(position + Vec {0.f, rounding[0]});
			auto a1 = ktc::CenterArc {
				position + Vec{rounding[0], rounding[0]},
				{rounding[0], rounding[0]},
				nytl::constants::pi,
				nytl::constants::pi * 1.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position);
		}

		// topLeft
		if(rounding[1] != 0.f) {
			dlg_assert(rounding[1] > 0.f);
			auto x = position.x + size.x - rounding[1];
			points.push_back({x, position.y});
			auto a1 = ktc::CenterArc {
				{x, position.y + rounding[1]},
				{rounding[1], rounding[1]},
				nytl::constants::pi * 1.5f,
				nytl::constants::pi * 2.f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + Vec {size.x, 0.f});
		}

		// bottomRight
		if(rounding[2] != 0.f) {
			dlg_assert(rounding[2] > 0.f);
			auto y = position.y + size.y - rounding[2];
			points.push_back({position.x + size.x, y});
			auto a1 = ktc::CenterArc {
				{position.x + size.x - rounding[2], y},
				{rounding[2], rounding[2]},
				0.f,
				nytl::constants::pi * 0.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + size);
		}

		// bottomLeft
		if(rounding[3] != 0.f) {
			dlg_assert(rounding[3] > 0.f);
			points.push_back({position.x + rounding[3], position.y + size.y});
			auto y = position.y + size.y - rounding[3];
			auto a1 = ktc::CenterArc {
				{position.x + rounding[3], y},
				{rounding[3], rounding[3]},
				nytl::constants::pi * 0.5f,
				nytl::constants::pi * 1.f,
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(position + Vec {0.f, size.y});
		}

		// close it
		points.push_back(points[0]);
		polygon_.update(points, state_.drawMode);
	}
}

void RectShape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool RectShape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

// CircleShape
CircleShape::CircleShape(Context& ctx,
	Vec2f xcenter, Vec2f xradius, const DrawMode& xdraw,
	unsigned xpoints, float xstartAngle)
		: state_{xcenter, xradius, std::move(xdraw), xpoints, xstartAngle},
			polygon_(ctx) {

	update();
	polygon_.updateDevice();
}

CircleShape::CircleShape(Context& ctx,
	Vec2f xcenter, float xradius, const DrawMode& xdraw,
	unsigned xpoints, float xstartAngle)
		: CircleShape(ctx, xcenter, {xradius, xradius},
			xdraw, xpoints, xstartAngle) {
}

void CircleShape::update() {
	dlg_assertl(dlg_level_warn, state_.pointCount > 2);

	std::vector<Vec2f> pts;
	pts.reserve(state_.pointCount);

	auto a = state_.startAngle;
	auto d = 2 * nytl::constants::pi / state_.pointCount;
	for(auto i = 0u; i < state_.pointCount + 1; ++i) {
		using namespace nytl::vec::cw::operators;
		auto p = Vec {std::cos(a), std::sin(a)} * state_.radius;
		pts.push_back(state_.center + p);
		a += d;
	}

	polygon_.update(pts, state_.drawMode);
}

void CircleShape::disable(bool d, DrawType t) {
	polygon_.disable(d, t);
}

bool CircleShape::disabled(DrawType t) const {
	return polygon_.disabled(t);
}

} // namespac rvg
