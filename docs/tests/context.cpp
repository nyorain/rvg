// Tests context and deviceObject creation, life/render-cycle and destruction.

#include <rvg/context.hpp>
#include <rvg/polygon.hpp>
#include "main.hpp"

TEST(basicSetup) {
	auto pctx = createContext();
	auto& ctx = *pctx;

	rvg::Polygon p1 {};
	EXPECT(p1.valid(), false);

	auto points = {nytl::Vec2f{1.f, 2.f,}, nytl::Vec2f{3.f, 4.f}};
	rvg::Polygon p2 {ctx};
	p2.update(points, {});
	EXPECT(p2.valid(), true);
	p2 = {};
	EXPECT(p2.valid(), false);

	rvg::Polygon p3 {ctx};
	p3.update(points, {false, 2.f});
	EXPECT(p3.valid(), true);
	EXPECT(p3.disabled(), false);
	EXPECT(p3.disabled(rvg::DrawType::stroke), false);
	EXPECT(p3.disabled(rvg::DrawType::fill), false);
	EXPECT(p3.disabled(rvg::DrawType::strokeFill), false);

	rvg::Polygon p4 {ctx};
	p4.disable(true);
	p4.update(points, {true});
	EXPECT(p4.disabled(rvg::DrawType::stroke), true);
	EXPECT(p4.disabled(rvg::DrawType::fill), true);
	EXPECT(p4.disabled(rvg::DrawType::strokeFill), true);
	EXPECT(p4.valid(), true);
	EXPECT(&p4.context(), &ctx);

	rvg::Polygon p5 {ctx};
	p5.disable(false);
	EXPECT(p5.disabled(rvg::DrawType::stroke), false);
	EXPECT(p5.disabled(rvg::DrawType::fill), false);
	EXPECT(p5.disabled(rvg::DrawType::strokeFill), false);

	rvg::Paint paint {ctx, rvg::colorPaint(rvg::Color::blue)};

	// Context::updateDevice does not return true when new DeviceObjects
	// are created since then the caller/creator knows he wants to render
	// something new and therefore rerecords. This also allows to create
	// paints/transforms/polygons that are just not used yet without
	// unneeded rerecording.
	// But here we also called update on polygons.
	// TODO: think about ctx.updateDevice() semantics for unused
	// device objects.
	// EXPECT(ctx.updateDevice(), false);
	ctx.updateDevice();


	// record and submit, we don't care about output here.
	// check that stuff is bound by default
	// we have to bind a paint though
	auto cmdBuf = record(ctx, [&](auto& di){
		paint.bind(di);
		p3.stroke(di);
		p4.fill(di);
	});

	renderSubmit(ctx, cmdBuf);
}
