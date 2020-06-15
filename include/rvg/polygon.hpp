// Copyright (c) 2018 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <rvg/fwd.hpp>
#include <rvg/deviceObject.hpp>

#include <nytl/vec.hpp>
#include <nytl/matOps.hpp>
#include <vpp/trackedDescriptor.hpp>
#include <vpp/sharedBuffer.hpp>

namespace rvg {

/// Specifies in which way a polygon can be drawn.
struct DrawMode {
	/// Whether polygon/shape can be filled.
	/// While this is set to false for a polygon/shape you must not
	/// use a command buffer that fills the polygon/shape.
	/// So you want to set this to true whenever the polygon might
	/// be filled. If you then don't want to fill it, use the disable
	/// functionality.
	bool fill {};

	/// Whether this polygon/shape can be stroked (and how thick).
	/// Semantics are similar to the fill attribute but you
	/// additionally have to specify the stroke thickness. Setting
	/// this to 0.f means that the shape/polygon must never be used
	/// for stroking. Otherwise its stroke data will be maintained
	/// for the given thickness so it can be stroked.
	/// Must not be negative.
	float stroke {};

	/// Whether to loop the stroked points
	/// Has no effect for filling.
	bool loop {};

	/// Defines per-point color values.
	/// If the polygon is then filled/stroked with a pointColorPaint,
	/// will use those points.
	struct {
		/// The per-point color values. Should have the same size
		/// as the points span passed to the polygon.
		/// As all colors, are expected to be in srgb space.
		std::vector<Vec4u8> points {};
		bool fill {}; /// whether they can be used when filling
		bool stroke {}; /// whether they can be used when stroking
	} color {};

	/// Whether to enable anti aliased fill
	/// Antialiasing must be enabled for the context.
	/// Changing this will always trigger a rerecord.
	/// May have really large performance impact.
	bool aaFill {};

	/// Whether to enable anti aliased stroking
	/// Antialiasing must be enabled for the context.
	/// Changing this will always trigger a rerecord.
	/// May have some performance impact but way less then aaFill.
	bool aaStroke {};

	/// Whether to use deviceLocal memory.
	/// Usually makes updates slower and draws faster so use this
	/// for polygons that don't change often but are drawn.
	/// If this is false, hostVisible memory will be used.
	bool deviceLocal {};

	/// Pre-transform that is applied while baking the primitive.
	/// Comparison to the post-transform state via rvg::Transform:
	/// - Every time the pre-transform changes the polygon has to be
	///   rebaked/updated. More expensive
	/// - Doing scaling via the post transform has serious problems:
	///   anti aliasing might break and for shapes (that use this
	///   pre transform as well) curves might not be correctly tesselated.
	/// TODO(v0.2)
	// nytl::Mat3f transform = nytl::identity<3, float>();
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
	/// Undefined behaviour if it was updated without fill support in
	/// the DrawMode.
	void fill(vk::CommandBuffer) const;

	/// Records commands to stroke this polygon into the given DrawInstance.
	/// Undefined behaviour if it was updates without stroke support in
	/// the DrawMode.
	void stroke(vk::CommandBuffer) const;

	/// Usually only automatically called from context when needed.
	/// Uploads data to the device. Must not be called while a command
	/// buffer drawing the Polygon is executing.
	/// Returns whether a command buffer rerecord is needed.
	bool updateDevice();

protected:
	struct Draw {
		std::vector<Vec2f> points;
		std::vector<Vec4u8> color;
		vpp::SubBuffer pBuf;
		vpp::SubBuffer cBuf;
	};

	struct Stroke : public Draw {
		std::vector<Vec2f> aa;
		vpp::SubBuffer aaBuf;
	};

	// - internal utility -
	bool upload(Draw&, bool disable, bool color);
	bool upload(Stroke&, bool disable, bool color, bool aa, float* mult);

	void updateStroke(Span<const Vec2f>, const DrawMode&);
	void updateFill(Span<const Vec2f>, const DrawMode&);

	void stroke(vk::CommandBuffer, const Stroke&, bool aa, bool color,
		vk::DescriptorSet, unsigned aaOff) const;

	bool checkResize(vpp::SubBuffer&, vk::DeviceSize needed,
		vk::BufferUsageFlags);

protected:
	struct {
		bool fill : 1;
		bool stroke : 1;
		bool colorFill : 1;
		bool colorStroke : 1;
		bool disableFill : 1;
		bool disableStroke : 1;
		bool aaFill : 1;
		bool aaStroke : 1;
		bool deviceLocal : 1;
	} flags_ {};

	Draw fill_;
	Stroke fillAA_;
	Stroke stroke_;
	vpp::TrDs strokeDs_;
	float strokeMult_ {};
};

} // namespace rvg
