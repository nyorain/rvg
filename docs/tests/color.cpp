// Tests various color conversions
// TODO:
//  - more (also random) hsv conversions
//  - hsl conversions (from/to rgb as well as hsv)
//  - test mix function
//  - make sure asserts are triggered for invalid inputs

#include <bugged.hpp>
#include <rvg/paint.hpp>

using namespace rvg;

TEST(hsv) {
	// rgb to hsv
	EXPECT(hsv({0u, 0u, 0u}), (Vec3u8{0u, 0u, 0u}));
	EXPECT(hsv({norm, 1.f, 1.f, 1.f}), (Vec3u8{0u, 0u, 255u}));

	EXPECT(hsv({norm, 1.f, 0.f, 0.f}), (Vec3u8{0u * 255u / 6u, 255u, 255u}));
	EXPECT(hsv({norm, 1.f, 1.f, 0.f}), (Vec3u8{1u * 255u / 6u, 255u, 255u}));
	EXPECT(hsv({norm, 0.f, 1.f, 0.f}), (Vec3u8{2u * 255u / 6u, 255u, 255u}));
	EXPECT(hsv({norm, 0.f, 1.f, 1.f}), (Vec3u8{3u * 255u / 6u, 255u, 255u}));
	EXPECT(hsv({norm, 0.f, 0.f, 1.f}), (Vec3u8{4u * 255u / 6u, 255u, 255u}));
	EXPECT(hsv({norm, 1.f, 0.f, 1.f}), (Vec3u8{5u * 255u / 6u, 255u, 255u}));

	// hsv to rgb
	EXPECT(hsvNorm(0.f, 0.f, 1.f), Color::white);
	EXPECT(hsvNorm(0.1234f, 0.f, 1.f), Color::white);
	EXPECT(hsvNorm(1.f, 0.f, 1.f), Color::white);

	EXPECT(hsvNorm(0.f, 0.f, 0.f), Color::black);
	EXPECT(hsvNorm(0.1234f, 0.f, 0.f), Color::black);
	EXPECT(hsvNorm(1.f, 0.f, 0.f), Color::black);

	EXPECT(hsv(0u * 255u / 6u, 255u, 255u), Color::red);
	EXPECT(hsv(2u * 255u / 6u, 255u, 255u), Color::green);
	EXPECT(hsv(4u * 255u / 6u, 255u, 255u), Color::blue);
}

TEST(u32) {
	auto u = u32rgba({255u, 20u, 10u, 123u});
	EXPECT(u >> 24 & 0xFF, 255u); // red
	EXPECT(u >> 16 & 0xFF, 20u); // green
	EXPECT(u >> 8 & 0xFF, 10u); // blue
	EXPECT(u >> 0 & 0xFF, 123u); // alpha

	auto c = u32rgba(0xAABBCCDD);
	EXPECT(c.rgba(), (Vec4u8{0xAAu, 0xBBu, 0xCCu, 0xDDu}));
}
