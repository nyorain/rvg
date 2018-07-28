# notes

This file is for implementation notes, snippets, ideas and discussions.

# Gradient texture computations

## With a gradient texture:

```glsl

	// This computation can be done at gradient compute time (host)
	vec2 uv2 = ...;
	float total = textureHeight * (uv2.y - uv1.y) + (uv2.x - uv1.x);

	// start uv, total from ubo
	vec2 uv1 = ...;
	float val = uv1.x + clamp(fac, 0, 1) * total;
	vec2 uv = vec2(fract(val), floor(val));
	return texture(tex, uv);
```

NOTE: will not work on the edges, so probably bad idea...
Or just use one line in the texture per gradient but we might waste
a lot of space then.
NOTE: also completely forgot about stop spacing... won't work!

## With a ssbo

```glsl
	// use if conditions alternatively for 0 or 1
	float fac = clamp(..., 0.001, 0.999);
	fac *= gradient.stops.length();
	uint id = floor(fac);
	return mix(gradient.stops[i
```

nope, we would need a for loop here due to variable stop spacing...
guess a texture with waste is the way to go...

## ssbo try #2

```glsl
	float fac = clamp(..., 0, 1) - gradient.stops[0].offset;
	if(fac <= 0) {
		return gradient.stop[0].color;
	}

	for(uint i = 1; i < gradient.stops.length(); ++i) {
		fac -= gradients.stops[i].offset;
		if(fac <= 0) {
			return mix(gradient.stops[i - 1].color, gradient.stops[i].color,
				-fac / gradients.stops[i].offset);
		}
	}
```

# scissor

```
/// Represents a scissor region that limits the drawing region.
/// The scissor is applied AFTER the bound transform.
/// Depending on the settings in Context, this will either be recorded
/// statically and therefore require a rerecord on every change, or
/// computed dynamically or
class Scissor {
public:
	Scissor(Context&, nytl::Rect2f = {0, 0, 1e5, 1e5});
	bool updateDevice(const Context& ctx, nytl::Rect2f scissor);
	void bind(const DrawInstance& di);

protected:
	vpp::BufferRange ubo_;
	nytl::Rect2f scissor;
};
```

```
layout(set = 3, binding = 0) uniform Scissor {
	vec2 off;
	vec2 extent;
} scissor;
```

/// Scissor that is applied before any transform, in polygon coordinates.
/// Don't confuse those coordinates with final window coordinates and a
/// conventional scissor, although they are in simple cases the same.


/// Defines how scissor works for a context.
/// Not hard coded since this is a trade off between functionality,
/// availability and performance. Defaults to fragment but trying
/// to enable shaderClipDistance and then using that can bring
/// real improvements.
enum class ScissorMode {
	/// Dynamically computes the scissor bounds in the fragment shader.
	/// Default mode.
	/// + always available
	/// + no rerecord required on scissor change
	/// + can be applied before transform
	/// - impact on performance
	fragment = 0,

	/// Requires the shaderClipDistance vulkan feature to be enabled.
	/// Uses clipping planes in the vertex shader.
	/// + should be pretty performant on most hardware/drivers
	/// + feature avilable on most implementations
	/// + no rerecord required on scissor change
	/// + can be applied before transform
	/// - requires an optional feature that may not be enabled in the device,
	///   so not always available
	clipDistance,

	/// Relies on vkCmdSetScissor.
	/// NOTE: this mode actually modifies the behaviour of Scissor and
	/// should only be used if you know what you are doing.
	/// + the most performant option, zero overhead
	/// + always available
	/// - can only be applied in window coordinates, not before transformation
	///   so the resulting scissor rect will always be axis aligned
	/// - command buffers have to be rerecorded on every scissor change
	cmd,
};

## Polygon cleanup

```cpp
// - IndirectDrawCommand (16 bytes total)
// - positions, Vec2f per vertex (4 bytes)
// - color (optional), Vec4u8 per vertex (4 bytes)
// If color changes or buffer is recreated: rerecord needed.
struct FillPolygon {
	std::vector<Vec2f> positions;
	std::vector<Color> colors;
	vpp::BufferRange buffer;
};

// - IndirectDrawCommand (16 bytes total)
// - positions, Vec2f per vertex (4 bytes)
// - color (optional: color), Vec4u8 per vertex (4 bytes)
// - aa uv (optional: aa), Vec2f per vertex (8 bytes)
// - aa stroke mult (optional, 4 bytes total)
// If color or aa changes or buffer is recreated: rerecord needed.
struct StrokePolygon {
	std::vector<Vec2f> positions;
	std::vector<Color> colors;
	std::vector<Vec2f> uv;
	vpp::BufferRange buffer;
};

bool updateDevice(Context&, FillPolygon&);
bool updateDevice(Context&, StrokePolygon&);
```


## try for pencil drawing api

```cpp
someShape.fillMask(cb);
someRect.strokeMask(cb);
transform2.bind(cb);
someRect.fillMask(cb);
somePaint.bind(cb);
ctx.drawMasked(cb);
```

- everything is added to mask until drawMasked is called
	- then mask is reset
- problem: drawMasked must draw fullscreen and cannot know the largest
  bounds (which may after all be quite small -> large performance loss)
- not sure if possible without normal drawing interfering

```cpp
rvg::Mask mask(ctx, cb);
someShape.fill(mask);
someRect.stroke(mask);
transform2.bind(cb);
someRect.fill(mask);
mask.draw(paint);
```

- problem with this: interface seems like there can be multiple masks at
  a time which is not the case
- intermixing calls with cb and mask that influence each other is bad interface
  design as well


Reintroducing rvg::DrawInstance that __must__ always be used:

```
auto di = ctx.beginDraw(cb);
someShape.maskFill(di);
someRect.maskStroke(di);
transform2.bind(di);
someRect.maskFill(di);
somePaint.bind(di);
di.drawMasked();
```

Seems like the best alternative. Takes the lightweight feel away though
(which makes sense since the abstraction over the pencil-based drawing
is rather large and also not cheap on performance i'd guess). But to make
it consistent, all functions using a CommandBuffer directly have to be abolished.

## discussing transforms

- scaling in transform is a real problem for all kind of things. Maybe only
  allow translation and rotation in Transform state? (and introduce pre-
  transforms/only pre-scale?); or instead of "allowing" in terms of restricting
  the api (and not use a plain transform matrix), just document the issues and
  let the user decide what to use?
- problem with pre + post transform: not too intuitive, could overwhelm new
  users, seem redundant and badly designed (which it may be)
  	- problem: only scale as pre-"transform" or full matrix again?
	  if full matrix: document that it is less performant that post transform
	- how to handle text? just add scale in Text::state?
- end coordinate system (from api pov): normalized vulkan space vs window space
  (vs frame-buffer space?)
  	- pretty much no use cases work in normalized space
	- nanovg uses window size; svg specifies canvas in pixels
- devicePixelRatio and where to apply it/where to set it?
- (later) support for 3D coords? For polygon-based classes already possible
  via manual pre-transform (no depth though) but issue for text
