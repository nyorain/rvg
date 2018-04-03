#include "colorPicker.hpp"
#include "gui.hpp"
#include <dlg/dlg.hpp>
#include <nytl/rectOps.hpp>

namespace vui {
namespace {

template<size_t D, typename T>
Vec<D, T> clamp(Vec<D, T> v, Vec<D, T> min, Vec<D, T> max) {
	for(auto i = 0u; i < D; ++i) {
		v[i] = std::clamp(v[i], min[i], max[i]);
	}

	return v;
}

} // anon namespace

ColorPicker::ColorPicker(Gui& gui, const Rect2f& bounds, const Color& start)
	: ColorPicker(gui, bounds, start, gui.styles().colorPicker) {
}

ColorPicker::ColorPicker(Gui& gui, const Rect2f& bounds, const Color& start,
		const ColorPickerStyle& style) : Widget(gui, bounds), style_(style) {

	auto hsv = hsvNorm(start);

	selector_ = {context(), style.padding, {}, {true, style.strokeWidth}};
	basePaint_ = {context(), vgv::colorPaint(hsvNorm(hsv[0], 1.f, 1.f))};
	hueMarker_ = {context(), {}, {style.hueWidth, style.hueMarkerHeight},
		{false, style.hueMarkerThickness}};
	colorMarker_ = {context(), {}, style.colorMarkerRadius,
		{false, style.colorMarkerThickness}, 6u};

	auto drawMode = DrawMode {false, style.hueWidth};
	drawMode.color.fill = false;
	drawMode.color.stroke = true;

	for(auto i = 0u; i < 7 + 1; ++i) {
		auto col = hsvNorm(i / 6.f, 1.f, 1.f);
		drawMode.color.points.push_back(col.rgba());
	}

	hue_ = {context(), {}, std::move(drawMode)};

	sGrad_ = {context(), {}};
	vGrad_ = {context(), {}};

	auto size = bounds.size;
	if(size == Vec {autoSize, autoSize}) {
		size = {230, 200};
	} else if(size.x == autoSize) {
		size.x = std::min(0.87f * size.y, 150.f);
	} else if(size.y == autoSize) {
		size.y = std::min(1.15f * size.x, 120.f);
	}

	this->size(hsv, size);
}

void ColorPicker::size(Vec2f size) {
	auto sv = currentSV();
	this->size({currentHue(), sv.x, sv.y}, size);
}

void ColorPicker::size(Vec3f hsv, Vec2f size) {
	auto es = size - 2 * style().padding; // effective size

	// selector
	auto sc = selector_.change();
	sc->size = es;
	sc->size.x -= style().hueWidth + style().huePadding;

	// hue
	auto hc = hue_.change();
	hc->points.clear();

	auto ystep = es.y / 6.f;
	auto x = style().padding.x + es.x - style().hueWidth / 2;
	for(auto i = 0u; i < 7; ++i) {
		hc->points.push_back({x, style().padding.y + ystep * i});
	}

	// hue marker
	auto hmc = hueMarker_.change();
	hmc->position.x = es.x - style().hueWidth;
	hmc->position.y = hsv[0] * es.y - style().hueMarkerHeight / 2.f;
	hmc->position += style().padding;

	// color marker
	using namespace nytl::vec::cw::operators;
	auto cmc = colorMarker_.change();
	cmc->center = Vec {hsv[1], 1.f - hsv[2]} * selector_.size();
	cmc->center += style().padding;

	// gradients
	sGrad_.paint(vgv::linearGradient(
		selector_.position(), Vec {selector_.size().x, 0.f},
		::vgv::hsv(0, 0, 255), ::vgv::hsv(0, 0, 255, 0)));
	vGrad_.paint(vgv::linearGradient(
		selector_.position(), Vec {0.f, es.y},
		::vgv::hsv(0, 255, 0, 0), ::vgv::hsv(0, 255, 0)));

	Widget::size(size);
}

void ColorPicker::hide(bool hide) {
	hue_.disable(hide);
	hueMarker_.disable(hide);
	selector_.disable(hide);
	colorMarker_.disable(hide);
}

bool ColorPicker::hidden() const {
	return hue_.disabled();
}

Widget* ColorPicker::mouseButton(const MouseButtonEvent& ev) {
	if(ev.button != MouseButton::left) {
		return nullptr;
	}

	sliding_ = ev.pressed;
	if(ev.pressed) {
		click(ev.position - position());
	}

	return this;
}

Widget* ColorPicker::mouseMove(const MouseMoveEvent& ev) {
	if(sliding_) {
		click(ev.position - position());
		return this;
	}

	return nullptr;
}

void ColorPicker::pick(const Color& color) {
	auto hsv = hsvNorm(color);

	auto hue = hsv[0];
	auto sv = Vec {hsv[1], hsv[2]};

	// update paints/shapes
	basePaint_.paint(colorPaint(hsvNorm(hue, 1.f, 1.f)));

	auto hmc = hueMarker_.change();
	hmc->position.y = style().padding.y + hue * selector_.size().y;
	hmc->position.y -= style().hueMarkerHeight / 2.f;

	using namespace nytl::vec::cw::operators;
	auto cmc = colorMarker_.change();
	cmc->center = sv * selector_.size();
	cmc->radius = {style().colorMarkerRadius, style().colorMarkerRadius};
}

void ColorPicker::draw(const DrawInstance& di) const {
	Widget::bindState(di);

	for(auto* p : {&basePaint_, &sGrad_, &vGrad_}) {
		p->bind(di);
		selector_.fill(di);
	}

	context().pointColorPaint().bind(di);
	hue_.stroke(di);

	if(style().stroke) {
		style().stroke->bind(di);
		selector_.stroke(di);
	}

	dlg_assert(style().marker);
	style().marker->bind(di);
	hueMarker_.stroke(di);
	colorMarker_.stroke(di);
}

void ColorPicker::click(Vec2f pos) {
	pos = clamp(pos, style().padding, size() - style().padding);
	auto hue = Rect2f {
		{selector_.size().x + style().huePadding, style().padding.y},
		{style().hueWidth, selector_.size().y}
	};

	if(nytl::contains(selector_.bounds(), pos)) {
		colorMarker_.change()->center = pos;
		if(onChange) {
			onChange(*this);
		}
	} else if(nytl::contains(hue, pos)) {
		auto norm = (pos.y - style().padding.y) / selector_.size().y;
		hueMarker_.change()->position.y = pos.y;
		hueMarker_.change()->position.y -= style().hueMarkerHeight / 2.f;
		basePaint_.paint(colorPaint(hsvNorm(norm, 1.f, 1.f)));
		if(onChange) {
			onChange(*this);
		}
	}
}

float ColorPicker::currentHue() const {
	auto pos = hueMarker_.position().y + style().hueMarkerHeight / 2.f;
	return (pos - style().padding.y) / selector_.size().y;
}

Vec2f ColorPicker::currentSV() const {
	using namespace nytl::vec::cw::operators;
	auto sv = (colorMarker_.center() - style().padding) / selector_.size();
	sv.y = 1 - sv.y;
	return sv;
}

Color ColorPicker::picked() const {
	auto sv = currentSV();
	return hsvNorm(currentHue(), sv.x, sv.y);
}

} // namespace vui
