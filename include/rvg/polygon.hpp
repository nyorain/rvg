// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>

#include <nytl/vec.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/sharedBuffer.hpp>

namespace rvg {

/// Specifies in which way a polygon can be drawn.
struct DrawMode {
	bool fill {}; /// Whether it can be filled
	float stroke {}; /// Whether it can be stroked

	/// Defines per-point color values.
	/// If the polygon is then filled/stroked with a pointColorPaint,
	/// will use those points.
	struct {
		std::vector<Vec4u8> points {}; /// the per-point color values
		bool fill {}; /// whether they can be used when filling
		bool stroke {}; /// whether they can be used when stroking
	} color {};
};

enum class DrawType {
	stroke,
	fill,
	strokeFill
};

/// A shape defined by points that can be stroked or filled.
class Polygon : public DeviceObject {
public:
	Polygon() = default;
	Polygon(Context&);

	/// Can be called at any time, computes the polygon from the given
	/// points and draw mode. The DrawMode specifies whether this polygon
	/// can be used for filling or stroking and their properties.
	/// Automatically registers this object for the next updateDevice call.
	void update(Span<const Vec2f> points, const DrawMode&);

	/// Changes the disable state of this polygon.
	/// Cheap way to hide/unhide the polygon, can be called at any
	/// time and will never trigger a rerecord.
	/// Automatically registers this object for the next updateDevice call.
	void disable(bool, DrawType = DrawType::strokeFill);
	bool disabled(DrawType = DrawType::strokeFill) const;

	/// Records commands to fill this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without fill support.
	void fill(const DrawInstance&) const;

	/// Records commands to stroke this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without stroke support.
	void stroke(const DrawInstance&) const;

	/// Usually only automatically called from context.
	/// Uploads data to the device. Must not be called while a command
	/// buffer drawing the Polygon is executing.
	/// Returns whether a command buffer rerecord is needed.
	bool updateDevice();

protected:
	struct {
		bool fill : 1;
		bool stroke : 1;
		bool colorFill : 1;
		bool colorStroke : 1;
		bool disableFill : 1;
		bool disableStroke : 1;
	} flags_ {};

	struct {
		std::vector<Vec2f> points;
		std::vector<Vec4u8> color;
		vpp::BufferRange pBuf; // indirect draw command and points
		vpp::BufferRange cBuf; // (optional) color

		struct {
			std::vector<Vec2f> points;
			std::vector<Vec4u8> color;
			std::vector<Vec2f> aa;
			vpp::BufferRange pBuf;
			vpp::BufferRange cBuf;
			vpp::BufferRange aaBuf; // (optional) aa uv
			vpp::DescriptorSet ds; // thickness ubo (from aabuf);
			float mult; // for aa
		} aa;
	} fill_;

	struct {
		std::vector<Vec2f> points;
		std::vector<Vec4u8> color;
		std::vector<Vec2f> aa;
		vpp::BufferRange pBuf; // indirect draw command and points
		vpp::BufferRange cBuf; // (optional) color
		vpp::BufferRange aaBuf; // (optional) aa uv
		vpp::DescriptorSet aaDs; // thickness ubo (from aabuf);
		float mult; // for aa
	} stroke_;
};

} // namespace rvg
