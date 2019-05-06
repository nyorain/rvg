// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <rvg/shapes.hpp>
#include <rvg/util.hpp>
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
	// TODO: can probably all be done easier... first multiply
	// pos and size with transforms and then use the multiplied
	// radius
	auto tp = [&](float x, float y) {
		auto p = state_.position + Vec2f{x, y};
		return multPos(state_.transform, p);
	};

	if(state_.rounding == std::array<float, 4>{}) {
		auto points = {
			tp(0, 0),
			tp(state_.size.x, 0),
			tp(state_.size.x, state_.size.y),
			tp(0, state_.size.y),
			tp(0, 0),
		};
		polygon_.update(points, state_.drawMode);
	} else {
		constexpr auto steps = 12u; // TODO: make dependent on size
		auto size = state_.size;
		std::vector<Vec2f> points;
		auto& rounding = state_.rounding;

		// topRight
		if(rounding[0] != 0.f) {
			dlg_assert(rounding[0] > 0.f);
			auto radius = Vec2f{rounding[0], rounding[0]};
			radius = multDir(state_.transform, radius);

			points.push_back(tp(0, rounding[0]));
			auto a1 = ktc::CenterArc {
				tp(rounding[0], rounding[0]), radius,
				nytl::constants::pi,
				nytl::constants::pi * 1.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(tp(0, 0));
		}

		// topLeft
		if(rounding[1] != 0.f) {
			dlg_assert(rounding[1] > 0.f);
			auto radius = Vec2f{rounding[1], rounding[1]};
			radius = multDir(state_.transform, radius);

			points.push_back(tp(size.x - rounding[1], 0.f));
			auto a1 = ktc::CenterArc {
				tp(size.x - rounding[1], rounding[1]), radius,
				nytl::constants::pi * 1.5f,
				nytl::constants::pi * 2.f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(tp(size.x, 0.f));
		}

		// bottomRight
		if(rounding[2] != 0.f) {
			dlg_assert(rounding[2] > 0.f);
			auto radius = Vec2f{rounding[2], rounding[2]};
			radius = multDir(state_.transform, radius);

			points.push_back(tp(size.x, size.y - rounding[2]));
			auto a1 = ktc::CenterArc {
				tp(size.x - rounding[2], size.y - rounding[2]), radius,
				0.f,
				nytl::constants::pi * 0.5f
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(tp(size.x, size.y));
		}

		// bottomLeft
		if(rounding[3] != 0.f) {
			dlg_assert(rounding[3] > 0.f);
			auto radius = Vec2f{rounding[3], rounding[3]};
			radius = multDir(state_.transform, radius);

			points.push_back(tp(rounding[3], size.y));
			auto a1 = ktc::CenterArc {
				tp(rounding[3], size.y - rounding[3]), radius,
				nytl::constants::pi * 0.5f,
				nytl::constants::pi * 1.f,
			};
			ktc::flatten(a1, points, steps);
		} else {
			points.push_back(tp(0, size.y));
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
	auto pcount = state_.pointCount;
	auto radius = multDir(state_.transform, state_.radius);
	auto center = multPos(state_.transform, state_.center);
	if(pcount == defaultPointCount) {
		pcount = std::min(8 + 8 * ((radius.x + radius.y) / 16), 256.f);
	}

	std::vector<Vec2f> pts;
	pts.reserve(pcount);

	auto a = state_.startAngle;
	auto d = 2 * nytl::constants::pi / pcount;
	for(auto i = 0u; i < pcount + 1; ++i) {
		using namespace nytl::vec::cw::operators;
		auto p = Vec {std::cos(a), std::sin(a)};
		pts.push_back(center + radius * p);
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
