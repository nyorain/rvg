# Intro to rvg

This provides a small intro - or getting started guide - for using rvg.
Feel free to ask any questions or bring on suggestions (github.com/nyorain/rvg).
Apart from this as an intro, the best source of documentation are the rvg
headers itself. If they leave any questions unanswered, please tell us with
a github issue.

## Goal

The goals of rvg is simple: provide a rather high level vulkan abstraction
for drawing 2 dimensional shapes. But instead of providing an immediate api,
rvg provides an object oriented retained mode api that can make full
use of vulkan performance (mainly/especially on host (cpu) side).
This can be useful e.g. for quick 2d prototypes or a vulkan gui library.
It can also be useful for small simple things like a quick start into
text rendering which is some effort to write from scratch (rvg reuses
the font handling from [nuklear](https://github.com/vurtun/nuklear)).

You can think of it like a more lightweight and (at least on host side)
more efficient counterpart to the graphics library of sfml (rvg does
*not* and will never include stuff like a window, input or audio
abstraction, libraries should be kept modular where possible IMO) for vulkan.
One difference is that rvg was more inspired by vector graphics oriented
libraries and concepts, providing gradients, pulling a svg-like path
library as dependency and implementing shape-based antialiasing (so you
don't have to use multisampling just to get e.g. a smooth ui).
Granted, rvg is probably a thinner abstraction in some places. You still
have to manually manage command buffers, queue submissions and synchronization.

The library is literately licensed under the Boost license
(see the LICENSE file) which does not even require you to include
the license when you just ship binaries and otherwise is comparable to MIT.

## Building

rvg is built modular and functionality that doesn't strictly belong in
this library is split into separate projects (most of them are written
by me as well so will be maintained while this project is).
When building with meson, those projects will automatically be downloaded
and compiled.

  - vulkan itself, naturally
  - vkpp: automatically generated c++ vulkan wrapper (header-only)
  	- used instead of vulkan-hpp to avoid the >40loc madness,
	  much more leightweight. I see that this could be a problem
	  for people already using vulkan-hpp so please report
	  ideas/suggestions/problems.
  - vpp: vulkan abstraction and utilities
  	- e.g. provides functionality for sharing device memory and buffer objects,
	  batching device submissions or upload to a buffer via a staging buffer
  - nytl: small general C++ utility, provides e.g. matrix and vector classes
  - katachi: computes vertex arrays from curves & shapes.
      - Used to compute stroke buffers, antialiase shapes and to compute
	  	rect roundings. You might want to use this to bake and render
	  	bezier curves or arcs. Provides a really lightweight svg-like path api.
  - dlg: really small c library used for logging and debugging (also
    by most of the other projects).

This may seem like quite a lot of dependencies, but this are all (those
projects require nothing else). It allows to develop rvg itself quite
modular and lightweight.
Building this library without meson is probably a bad idea.
You will also need a solid C++17 compiler for these projects and rvg itself,
which means msvc will not work at its current state (early 2018).
MinGW 64 on windows is tested.

Since rvg makes extensive use of the abstractions provided by vpp,
you will need to use some of them. But vpp also offers functionality
to create non-owned wrapper around a raw vulkan resources that allows
to use rvg.

The tests additionally use the bugged.hpp header for unit testing and
the example uses ny as window abstraction library.

## Basic concepts

rvg was directly build with vulkan in mind, meaning it can use vulkans
advantages quite well. When just rendering static shapes, you will
never need to rerecord the command buffers. But even things like changing
the color of a shape, a transform, scissor or even the points of
the shape itself can work without triggering a rerecord.
And rendering data (like points/colors/transforms/uv coords) is computed
and uploaded the gpu exactly once, not every frame which makes rvg
additionally more performant than immediate mode rendering (as e.g.
the immediate mode gui libs or rendering libs like nanovg do).

So the first thing you need is a 'rvg::Context'. That is associated
with a vulkan device, will create all needed layouts and pipelines and also
manages efficient data uploading. Aside from the vulkan device, you have
to pass the Context that renderPass and its subpass to use for rendering
(although this might be somewhat limiting, it is needed for pipeline creation).
There are much more settings (antiAliasing/a pipelineCache to use/whether
the clipDistance feature is enabled for faster scissors), see rvg/context.hpp
for a complete reference of ContextSettings.

After you have a context, you can create new shapes (like RectShape, CircleShape or
their lower-level alternative Polygon), Text objects, Paints, Transforms,
Scissors or Fonts (inside a FontAtlas) at any time. You can also update them
at any time, e.g. changing the size or position of a RectShape, the color of a paint
or the stroke with of a Polygon.
Then, when recording your command buffers, you can simply render/bind those
objects. Calls like Paint::bind or Transform::bind will usually directly
result in a descriptor set bind on the given command buffer, while
calls like RectShape::stroke or Text::draw will result in draw calls using
the bind state. So far everything should be the usual functionality
that you would expect.

Now to the rvg vulkan-specific parts. Once a frame, you have to call
Context::updateDevice which will return whether a command buffer
rerecord is needed. Unlike the other calls, this calls MUST only be made,
when no command buffer using resources of that context is currently
rendering. This function will update the rendering resources' data
on the device, which cannot be done during rendering. It will also
queue uploading/general work command buffers on the vpp::QueueSubmitter
and return a semaphore to this work if there was any work (otherwise
it will return a null handle). The caller must then simply
make sure that the QueueSubmitter submits all queued work and completes
the upload command buffers before rendering is started (for
this purpose the semaphore can be used, wait for it simply using
the allGraphics stage when submitting you rendering command buffers).

This allows you to easily integrate rvg rendering into any rendering
engine. A simple pseudo-codish example below:

```cpp
// Assuming this is the function in which you initialize the stuff
// you want to render. As mentioned, rvg objects can be created
// and destroyed at (almost) any point.
void init(vpp::Device& dev, vk::RenderPass rp, unsigned subpass) {
	rvg::ContextSettings settings {rp, subpass};
	rvg::Context ctx(dev, settings); // rvg::Context is non movable

	// create some shapes, paints, transforms, texts...
	// fontAtlas = rvg::FontAtlas(ctx);
	// font = rvg::Font(fontAtlas, "OpenSans.ttf", 13u);
	// text = rvg::Text(ctx, "Hello World", font, position);
	// paint = rvg::Paint(ctx, rvg::colorPaint(rvg::Color::white));
	// transform = rvg::Transform(ctx, transformMatrix);

	// rvg::DrawMode drawMode;
	// drawMode.fill = true; // We may want to fill the shape
	// drawMode.stroke = 2.f; // We may want to stroke it; 2.f width
	// drawMode.loop = true; // loop the stroke shape (close it)
	// rectShape = rvg::RectShape(ctx, pos, size, drawMode);
	// ...
}


// Assuming this function changes some (previosly created) rvg objects.
// Updating rvg objects makes use of an StateChange RAII idiom
// to only once really update the object after all changes.
void update() {
	text.change()->position = {100.f, 200.f};
	paint.paint(rvg::linearGradient(startPos, endPos, startCol, endCol));

	// When changing multiple fields, use a local StateChange
	auto rc = rectShape.change();
	rc->position += rvg::Vec2f {10.f, -10.f};
	rc->size = {500.f, 100.f};
	rc->rounding = {4.f, 4.f, 0.f, 0.f}; // clockwise, starting top left
	rc->drawMode.fill = true; // we might record fill commands for this shape
	rc->drawMode.stroke = 0.f; // we guarantee we don't stroke it

	// when the StateChange object (from rectShape.change()) goes out of
	// scope, it will update the shape for the accumulated changes only
	// once instead of e.g. rebaking the antialiased shapes for
	// every single change separately.
}


// Assuming this function records a given command buffer and we use it to
// draw our created shapes.
void record(vk::CommandBuffer cmdBuf) {
	// - command buffer setup is up to you -
	// vk::beginCommandBuffer(cmdBuf, ...);
	// vk::cmdStartRenderPass(cmdBuf, ...);
	// e.g. bind dynamic state
	// ...

	// - drawing using rvg -
	// We receive a rvg::DrawInstance from the context which
	// can be used to draw stuff. Basically just a wrapper
	// around the passed vk::CommandBuffer.
	auto di = ctx.record(cmdBuf);

	// bind state (i.e. DescriptorSets)
	scissor.bind(di);
	paint.bind(di);
	transform.bind(di);

	text.draw(di); // draw a text object with bound paint, transform, scissor

	// if we want to fill a shape, its drawMode (while the commandBuffer
	// is used) must always have fill set to true which means it will
	// upadte the used fill buffer.
	rectShape.fill(di);

	// Similar to filling, while a commandBuffer that strokes a shape
	// is active, its drawMode must have the stroke field to a
	// strictly positive value, thereby also controlling the stroke width.
	circleShape.stroke(di);

	// ...

	// - again up to you -
	// vk::endRenderPass(cmdBuf);
	// vk::endCommandBuffer(cmdBuf);
}


// We assume this is the function implementing a rendering loop
void mainLoop() {
	// setup stuff ...

	while(!exit) {
		// - update device and render -
		auto [rerecord, semaphore] = ctx.upload();
		if(rerecord) {
			// rerecord command buffer(s)
			// none of the command buffers previously recorded that
			// reference rvg device objects must be used anymore
			rerecordCommandBuffers();
		}

		// Submit your own command buffers for rendering but make
		// sure any rendering waits on the semaphore.
		// To keep the amount of vk::queueSubmit calls to a minimum (since
		// they are considered expensive) rvg uses vpp's work batching
		// mechanism (implemented as vpp::QueueSubmitter class).
		// If you don't use this, you have to submit the QueueSubmitter
		// used by rvg (the default vpp::Device::queueSubmitter()).
		submitAndPresent(semaphore);

		// - host-side update -
		// do stuff like updating logical state, receiving input
		// or updating rvg shapes. Best to put cpu expensive stuff here since
		// this time is basically free. Stuff that can be run
		// while the device is busy rendering (and therefore keeping
		// the frametime short). If you don't have a main loop
		// optimized for this level of parallel execution and don't use the cpu
		// anything while a rendering command buffer is executed, you
		// can call ctx.updateDevice at any time and don't have to separate
		// between update and updateDevice frame sections.
		updateLogicalState();

		// - wait for rendering to finish -
		// e.g. when using vpp::QueueSubmitter you previously would have
		// recevied a submit id and now simply wait for the completion
		// of the submissions
		queueSubmitter.wait(submitID);
	}
}
```

You can clearly see that the rvg interface was highly optimized for efficient
vulkan render loops, where you don't simply stall the cpu while waiting
for the device to finish but use it. If used like this, rvg will perform
all possible work (like computing curves, antialiased shape or stroke data,
or filling staging buffers and recording their upload command buffers) while
the device is busy rendering.

## Synchronization

There is another thing you have to care about when rendering using rvg:
correct device-side synchronization. The command buffer that renders
using rvg must in some way signal vulkan that it requires the content
written into buffers/images by the rvg staging buffer/rvg memory maps.
The recommended and really simple way to do this is to add an external render
pass dependency. If you also render using any other resources that you
might update during rendering you would already have to care about this
and might already have (or might have needed) such a barrier.

```cpp
vk::SubpassDependency dependency;
dependency.srcSubpass = vk::subpassExternal;
dependency.dstSubpass = // the subpass you use rvg in, e.g. 0.
dependency.srcStageMask =
	vk::PipelineStageBits::host |
	vk::PipelineStageBits::transfer;
dependency.srcAccessMask = vk::AccessBits::hostWrite |
	vk::AccessBits::transferWrite;
dependency.dstStageMask = vk::PipelineStageBits::allGraphics;
dependency.dstAccessMask = vk::AccessBits::uniformRead |
	vk::AccessBits::vertexAttributeRead |
	vk::AccessBits::indirectCommandRead |
	vk::AccessBits::shaderRead;
```

Other than that you should not have to care about synchronization.
But no rvg classes are internally synchronized so they should always
just be used from one thread. This strict requirement will probably be
relaxed in a future version, you can probably already call update, draw/bind
and updateDevice on Polygon/Shape/Text/Paint etc from any thread as long
as there is never more than one thread calling it. But this was not fully
tested yet.
