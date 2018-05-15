#include <rvg/context.hpp>
#include <rvg/polygon.hpp>
#include <rvg/shapes.hpp>
#include "main.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

TEST(rect) {
	auto pctx = createContext();
	auto& ctx = *pctx;

	auto paint = rvg::Paint(ctx, rvg::colorPaint(rvg::Color::blue));
	auto shape = rvg::RectShape(ctx, {-1.f, -1.f}, {2.f, 2.f},
		{true, 0.f}, {0.5f, 0.5f, 0.5f, 0.5f});

	vpp::SubBuffer img;
	ctx.updateDevice();
	auto cmdBuf = record(ctx, [&](auto& cb){
		ctx.bindDefaults(cb);
		paint.bind(cb);
		shape.fill(cb);
	}, [&](auto& cb) {
		img = readImage(cb);
	});

	renderSubmit(ctx, cmdBuf);

	auto map = img.memoryMap();
	stbi_write_png("1.png", fbExtent.width, fbExtent.height, 4u,
		map.ptr(), fbExtent.width * 4u);
}
