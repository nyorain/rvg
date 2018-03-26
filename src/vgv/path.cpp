#include <vgv/path.hpp>
#include <nytl/vecOps.hpp>
#include <nytl/approxVec.hpp>
#include <dlg/dlg.hpp>
#include <cmath>

// https://svgwg.org/svg2-draft/paths.html#DProperty
// https://www.w3.org/TR/SVG11/paths.html#PathElement

namespace vgv {
namespace {

/// 2-dimensional cross product.
/// Is the same as the dot of a with the normal of b.
template<typename T>
constexpr T cross(nytl::Vec<2, T> a, nytl::Vec<2, T> b) {
    return a[0] * b[1] - a[1] * b[0];
}

/// Mirrors the given point on the given mirror.
template<std::size_t D, typename T>
constexpr Vec<D, T> mirror(const Vec<D, T>& mirror, const Vec<D, T>& point) {
	return mirror + (mirror - point);
}

/// Returns the left/right normal of a 2d vector
inline nytl::Vec2f lnormal(nytl::Vec2f vec) { return {-vec[1], vec[0]}; }
inline nytl::Vec2f rnormal(nytl::Vec2f vec) { return {vec[1], -vec[0]}; }

/// Simple Paul de Casteljau implementation.
/// See antigrain.com/research/adaptive_bezier/
void subdivide(const CubicBezier& bezier, unsigned maxlvl, unsigned lvl,
		std::vector<nytl::Vec2f>& points) {

	constexpr auto minSubdiv = 0.00001; // TODO
	if (lvl > maxlvl) {
		return;
	}

	auto p1 = bezier.start;
	auto p2 = bezier.control1;
	auto p3 = bezier.control2;
	auto p4 = bezier.end;

	auto p12 = 0.5f * (p1 + p2);
	auto p23 = 0.5f * (p2 + p3);
	auto p34 = 0.5f * (p3 + p4);
	auto p123 = 0.5f * (p12 + p23);

	auto d = p4 - p1;
	auto d2 = std::abs(cross(p2 - p4, d));
	auto d3 = std::abs(cross(p3 - p4, d));

	if((d2 + d3) * (d2 + d3) <= minSubdiv * dot(d, d)) {
		points.push_back(p4);
		return;
	}

	auto p234 = 0.5f * (p23 + p34);
	auto p1234 = 0.5f * (p123 + p234);

	subdivide({p1, p12, p123, p1234}, maxlvl, lvl + 1, points);
	subdivide({p1234, p234, p34, p4}, maxlvl, lvl + 1, points);
}

} // anon namespace

// stackoverflow.com/questions/3162645/convert-a-quadratic-bezier-to-a-cubic
CubicBezier quadToCubic(const QuadBezier& b) {
	return {b.start,
		b.start + (2 / 3.f) * (b.control - b.start),
		b.end + (2 / 3.f) * (b.control - b.end),
		b.end};
}

void bake(const CubicBezier& bezier, std::vector<Vec2f>& p, unsigned maxLevel) {
	subdivide(bezier, maxLevel, 0, p);
}

void bake(const QuadBezier& bezier, std::vector<Vec2f>& p, unsigned maxLevel) {
	return bake(quadToCubic(bezier), p, maxLevel);
}

// Arc implementations from
// www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
void bake(const CenterArc& arc, std::vector<Vec2f>& points, unsigned steps) {
	// Currently no x-axis rotation possible
	auto delta = arc.end - arc.start;
	points.reserve(points.size() + steps);
	for(auto i = 1u; i <= steps; ++i) {
		auto angle = arc.start + i * (delta / steps);
		points.push_back({
			arc.radius.x * float(cos(angle)) + arc.center.x,
			arc.radius.y * float(sin(angle)) + arc.center.y,
		});
	}
}

CenterArc parseArc(Vec2f from, const ArcParams& params, Vec2f to) {
	auto r = params.radius;

	// step 1 (p = (x', y'))
	auto p = 0.5f * (from - to);

	// step2 (tc = (cx', cy'))
	auto rxs = r.x * r.x, rys = r.y * r.y, pys = p.y * p.y, pxs = p.x * p.x;
	auto inner = (rxs * rys - rxs * pys - rys * pxs) / (rxs * pys + rys * pxs);
	auto sign = (params.largeArc != params.clockwise) ? 1 : -1;
	auto mult = Vec {r.x * p.y / r.y, -r.y * p.x / r.x};
	auto tc = sign * std::sqrt(inner) * mult;

	// step3: center
	auto c = tc + Vec2f{(from.x + to.x) / 2, (from.y + to.y) / 2};

	// step4: angles
	auto vec1 = Vec {(p.x - tc.x) / r.x, (p.y - tc.y) / r.y};
	auto vec2 = Vec {(-p.x - tc.x) / r.x, (-p.y - tc.y) / r.y};
	auto angle1 = angle(Vec2f{1, 0}, vec1);
	auto delta = float(std::fmod(angle(vec1, vec2), 2 * nytl::constants::pi));

	if(!params.clockwise && delta > 0) {
		delta -= 2 * nytl::constants::pi;
	} else if(params.clockwise && delta < 0) {
		delta += 2 * nytl::constants::pi;
	}

	return {c, params.radius, angle1, angle1 + delta};
}

std::vector<Vec2f> bake(const Subpath& sub) {
	// The number of points used to bake a full circle.
	// Smaller arcs scaled linearly
	// NOTE: we could also take the arc size (i.e. radius) into account
	constexpr auto arcCircleSteps = 64;

	if(sub.commands.empty()) {
		return {};
	}

	std::vector<Vec2f> points = {sub.start};
	points.reserve(sub.commands.size() * 2);

	auto current = sub.start;
	auto lastControlQ = current;
	auto lastControlC = current;
	auto to = current;

	auto commandBaker = [&](auto&& params){
		using T = std::decay_t<decltype(params)>;

		if constexpr(std::is_same_v<T, LineParams>) {
			points.push_back(to);
			lastControlC = lastControlQ = to;
		} else if constexpr(std::is_same_v<T, QBezierParams>) {
			bake(QuadBezier {current, params.control, to}, points);
			lastControlQ = params.control;
			lastControlC = to;
		} else if constexpr(std::is_same_v<T, SQBezierParams>) {
			lastControlQ = mirror(current, lastControlQ);
			bake(QuadBezier {current, lastControlQ, to}, points);
			lastControlC = to;
		} else if constexpr(std::is_same_v<T, CBezierParams>) {
			bake({current, params.control1, params.control2, to}, points);
			lastControlQ = to;
			lastControlC = params.control2;
		} else if constexpr(std::is_same_v<T, SCBezierParams>) {
			auto control = mirror(current, lastControlC);
			bake({current, control, params.control2, to}, points);
			lastControlQ = to;
			lastControlC = params.control2;
		} else if constexpr(std::is_same_v<T, ArcParams>) {
			auto arc = parseArc(current, params, to);
			auto fac = std::abs(arc.end - arc.start) / (2 * nytl::constants::pi);
			auto steps = arcCircleSteps * fac;
			bake(arc, points, steps);
			lastControlC = lastControlQ = to;
		} else {
			dlg_error("bake(Subpath): Invalid variant");
			return;
		}
	};

	for(auto& cmd : sub.commands) {
		to = cmd.to;
		visit(commandBaker, cmd.params);
		current = to;
	}

	if(sub.closed) {
		points.push_back(sub.start);
	}

	return points;
}

Subpath parseSvgSubpath(nytl::Vec2f start, nytl::StringParam svg) {
	// TODO: error checks etc
	//  - check if strtol succeeded.
	//  - check for nonnegative numbers (e.g. radius), flags
	//  - handle arc axis rotation

	Subpath ret;
	ret.start = start;

	auto it = svg.c_str();
	dlg_assert(it);
	auto end = const_cast<char**>(&it);

	auto readCoords = [&]{
		return Vec2f {
			// float(strtol(it, end, 10)),
			// float(strtol(it, end, 10)),
			std::strtof(it, end),
			std::strtof(it, end),
		};
	};

	auto pushCmd = [&](const Vec2f& p, const auto& params) -> Command& {
		ret.commands.push_back({p, {params}});
		return ret.commands.back();
	};

	while(*it) {
		auto last = start;
		if(!ret.commands.empty()) {
			last = ret.commands.back().to;
		}

		while(isspace(*it)) {
			++it;
		}

		auto c = *it;
		++it;
		switch(c) {
			case 'M': case 'm': {
				dlg_error("Moving not allowed for subpath");
				return {};
			} case 'L': case 'l': {
				auto& cmd = pushCmd(readCoords(), LineParams {});
				if(c == 'l') {
					cmd.to += last;
				}
				break;
			} case 'H': case 'h': {
				auto& cmd = pushCmd({}, LineParams {});
				cmd.to.x = std::strtol(it, end, 10);
				cmd.to.y = last.y;
				if(c == 'h') {
					cmd.to.x += last.x;
				}
				break;
			} case 'V': case 'v': {
				auto& cmd = pushCmd({}, LineParams {});
				cmd.to.x = last.x;
				cmd.to.y = std::strtol(it, end, 10);
				if(c == 'v') {
					cmd.to.y += last.y;
				}
				break;
			} case 'C': case 'c': {
				auto& cmd = pushCmd({}, CBezierParams{});
				auto& bezier = std::get<CBezierParams>(cmd.params);
				bezier.control1 = readCoords();
				bezier.control2 = readCoords();
				cmd.to = readCoords();
				if(c == 'c') {
					bezier.control1 += last;
					bezier.control2 += last;
					cmd.to += last;
				}
				break;
			} case 'S': case 's': {
				auto& cmd = pushCmd({}, SCBezierParams{});
				auto& bezier = std::get<SCBezierParams>(cmd.params);
				bezier.control2 = readCoords();
				cmd.to = readCoords();
				if(c == 's') {
					bezier.control2 += last;
					cmd.to += last;
				}
				break;
			} case 'Q': case 'q': {
				auto& cmd = pushCmd({}, QBezierParams{});
				auto& bezier = std::get<QBezierParams>(cmd.params);
				bezier.control = readCoords();
				cmd.to = readCoords();
				if(c == 'q') {
					bezier.control += last;
					cmd.to += last;
				}
				break;
			} case 'T': case 't': {
				auto& cmd = pushCmd({}, SQBezierParams{});
				cmd.to = readCoords();
				if(c == 't') {
					cmd.to += last;
				}
				break;
			} case 'A': case 'a': {
				auto& cmd = pushCmd({}, ArcParams {});
				auto& arc = std::get<ArcParams>(cmd.params);
				arc.radius = readCoords();

				std::strtol(it, end, 10); // TODO: axisRotation
				arc.largeArc = std::strtol(it, end, 10);
				arc.clockwise = std::strtol(it, end, 10);

				cmd.to = readCoords();
				if(c == 'a') {
					cmd.to += last;
				}

				break;
			} case 'Z': case 'z': {
				ret.closed = true;
				break;
			} default: {
				dlg_error("Invalid subpath command {}", *it);
				return {};
			}
		}
	}

	return ret;
}

// stroke api
std::vector<Vec2f> bakeStroke(Span<const Vec2f> p, const StrokeSettings& s) {
	std::vector<Vec2f> ret;
	bakeStroke(p, s, ret);
	return ret;
}

std::vector<Vec2f> bakeStroke(const Subpath& sub, const StrokeSettings& s) {
	auto points = bake(sub);
	return bakeStroke(points, s);
}

void bakeStroke(Span<const Vec2f> points, const StrokeSettings& settings,
		std::vector<Vec2f>& ret) {

	dlg_assert(settings.width > 0.f);

	auto width = settings.width * 0.5f;
	if(points.size() < 2) {
		return;
	}

	auto loop = points.front() == points.back();
	if(loop) {
		points = points.slice(0, points.size() - 1);
	}

	auto p0 = points.back();
	auto p1 = points.front();
	auto p2 = points[1];

	for(auto i = 0u; i < points.size() + loop; ++i) {
		auto d0 = lnormal(p1 - p0);
		auto d1 = lnormal(p2 - p1);

		if(i == 0 && !loop) {
			d0 = d1;
		} else if(i == points.size() - 1 && !loop) {
			d1 = d0;
		}

		// skip point if same to next or previous one
		// this assures normalized below will not throw (for nullvector)
		if(d0 == approx(Vec {0.f, 0.f}) || d1 == approx(Vec {0.f, 0.f})) {
			dlg_debug("bakeStroke: doubled point {}", p1);
			p1 = p2;
			p2 = points[i + 2 % points.size()];
			continue;
		}

		auto extrusion = 0.5f * (normalized(d0) + normalized(d1));

		ret.push_back(p1 + width * extrusion);
		ret.push_back(p1 - width * extrusion);

		p0 = points[(i + 0) % points.size()];
		p1 = points[(i + 1) % points.size()];
		p2 = points[(i + 2) % points.size()];
	}
}

void bakeColoredStroke(Span<const Vec2f> points, Span<const Vec4u8> color,
		const StrokeSettings& settings, std::vector<Vec2f>& outPoints,
		std::vector<Vec4u8>& outColor) {

	dlg_assert(color.size() == points.size());
	dlg_assert(settings.width > 0.f);

	auto width = settings.width * 0.5f;
	if(points.size() < 2) {
		return;
	}

	auto loop = points.front() == points.back();
	if(loop) {
		points = points.slice(0, points.size() - 1);
	}

	auto p0 = points.back();
	auto p1 = points.front();
	auto p2 = points[1];

	for(auto i = 0u; i < points.size() + loop; ++i) {
		auto d0 = lnormal(p1 - p0);
		auto d1 = lnormal(p2 - p1);

		if(i == 0 && !loop) {
			d0 = d1;
		} else if(i == points.size() - 1 && !loop) {
			d1 = d0;
		}

		// skip point if same to next or previous one
		// this assures normalized below will not throw (for nullvector)
		if(d0 == approx(Vec {0.f, 0.f}) || d1 == approx(Vec {0.f, 0.f})) {
			p1 = p2;
			p2 = points[i + 2 % points.size()];
			continue;
		}

		auto extrusion = 0.5f * (normalized(d0) + normalized(d1));

		outPoints.push_back(p1 + width * extrusion);
		outPoints.push_back(p1 - width * extrusion);

		outColor.push_back(color[i]);
		outColor.push_back(color[i]);

		p0 = points[i];
		p1 = points[i + 1 % points.size()];
		p2 = points[i + 2 % points.size()];
	}
}

} // namespace vgv
