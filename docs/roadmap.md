## Roadmap

### v0.2

- [ ] fix this terrible srgb conversion
- [ ] allow to specify font align, see the fons options
      document how this effects quried bounds etc
- [ ] allow float color in Paint? maybe add float color class?
- [x] better font handling: automatic atlas rebaking, fallback fonts
- [ ] text __scaling__
	- [ ] allow to set fonstash attributes like blur, spacing?
- [ ] allow to specify fringe on context creation/change it later
	- [ ] __scaling__ with transform?
- [x] care about texture srgb. You probably want srgb textures.
      _yep, were needed since most images use srgb/stb_image outputs srgb_
	- [x] add srgb support for vpp/formats (?) [wasn't needed]
- [ ] make positioning textures easier (NO manual matrix...)
	- [ ] add utility functions that create the matrix (from translate,
	      scale rotation)
	- [ ] allow specifying uv coords for textures?
		  as alternative to the texture transforms
		  optionally in polygon
- [ ] strokeWidth < fringeWidth (nvg line 2258), github.com/memononen/nanovg/issues/68
- [ ] remove code duplication in ny/glfw backends. Define app and such
      only once (or at least the widgets once)

problem of __scaling__ should be solved in this release:
	When using a non-window transform (e.g. level transform) then
	fringe/text size/whatever will not work anymore since they are currently
	given in pre-transform coords. Introduce context-wide scaling/
	per-object scaling/Scaling state? What about stroke width?
	nanovg solves it with a "devicePixelRatio" and seperate.
	(and by doing transform BEFORE doing stuff like tesselation...)
	tesselation (-> katachi) is affected by the same problem (problem
	of rvg?)
	See scale.cpp. Currently has still problems with rotations for shapes
	hard to fix, maybe only allow scale?
Completely abolish transform state?
	well, rotation and translation aren't a problem. Only scale.
	Maybe allow to set pre-transform for each polygon? On change,
	the whole polygon/text has to rebaked.

- [ ] clean up/solve transform state handling (scaling!)
- [ ] cleanup fontstash (and remove fso.h)
- [ ] better srgb conversion functions

### later

- [ ] general transform matrix on paint?
- [ ] corner bevels. Currently anti aliasing not too good for sharp corners
      -> katachi
- [ ] rvg: more stroke settings: linecap/linejoin [complex; -> katachi]
- [ ] when vui 0.1 is released/made public:
	- [ ] link to it in the readme as somewhat larger project using rvg
	- [ ] add highly functional and good looking example(s) using rvg and vui

- [ ] (improve) heavily documented basic example
	- [ ] try to use every feature once (in extra functions/modules)
- [ ] more (and systematic) rvg testing
	- [ ] some color conversion, mixing tests
	- [ ] test where all shapes are drawn once with possible settings
	      and paints and stuff
	- [ ] especially test defined behaviour when moving/destructing
	      objects
- [ ] check if DeviceObject/DevRes impl mess in Context can be cleaned up
	- [ ] currently rather error prone and (over-?)complicated with visitors
- [ ] custom queueSubmitter in ContextSettings. Or option for another
      submission method (callback + queueFamily or sth. that defaults
	  to current impl with default queue family and queue submitter)

	// was like this as todo in ContextSettings
	/// The QueueSubmitter to submit any upload work to.
	/// Should be associated with the queue for rendering since
	/// otherwise Context::waitSemaphore cannot be used for rendering.
	/// In this case, one could still synchronize using fences/deviceWait.
	// vpp::QueueSubmitter& submitter;
- [ ] bind initial paint that simply has dummy texture pattern to signal
      that no paint is bound?
- [ ] make non-texture gradients make use of transform buffer span
- [ ] nanovg-like box gradient
- [ ] helper for non-convex shapes (stencil buffer? or decomposition?)
	- [ ] evaluate first if this makes sense for the scope of rvg. It might not
- [ ] radial gradients (allowing e.g. color wheel)
	- [ ] any other gradient types to implement?
- [ ] multistop gradients (?), using small 1d textures
	- [ ] see discussion https://github.com/memononen/nanovg/pull/430
- [ ] benchmark alternative pipelines, optimize default use cases
	- [ ] benchmark how much paints are slowing things down. I suspect
        that if we would get rid of gradients (and therefore always
		be able to use color paint; use plain uv for textures) and
		therefore paints; also then drawing everything with just one
		(or at least less) (indirect) draw commands, we could get
		a massive performance boost. Like only one vkDrawIndexedIndirect
		class or sth
  - [ ] performance optimizations, resolve performance todos
  - [ ] better with more (but also more optimized) pipelines?

Should we make sure that there is always only (at max) one StateChange
object for a polygon/shape/etc? Should not be needed but be
aware that multiple objects is not well tested and MIGHT
cause unexpected behavior.

~StateChange: we might actually rethrow the exception if there
is no active exception.

TODO(performance): use own BufferAllocator for staging buffer in context
  and clear all allocations on frame end. Also own DescriptorAllocator?
TODO(performance): use direct write for small updates in upload140?
TODO(performance): reduce number of SubBuffers? In text/polygon.
  will probably require the vpp::offset feature for upload140

TODO(performance): cache points vec in {Circle, Rect}Shape::update


We could record upload command buffers in update and not updateDevice.
But not sure if worth it, we then would have to use additional
mechanics to make sure only one upload command buffer per object
is used (so they are reused for multiple update calls).
Since multiple update calls are not uncommon, this might
not be a good idea.

### previously done

- [x] windows (basic panes, no operations)
- [x] widget sizing options, Window::create auto sizing
  - [x] auto sizing on construction
- [x] row layouts
- [x] slider
- [x] rvg: rounded rect
- [x] fix widget resizing todos
- [x] rvg: sanity checking (with asserts/logs)
  - [x] color functions, conversion
  - [x] shape drawing functions
- [x] rvg: antialiasing
  - [x] fix stroke caps
  - [x] better settings (Context, DrawMode)
  - [x] simplify Polygon
- [x] rvg: paint/transform/scissor on deviceLocal memory
  - [x] probably enough when specifiable on construction, must not be dynamic
  - [x] also text
  - [x] more efficient staging writes. Don't submit command buffer at once
    - [x] use semaphores (-> vpp: work chaining)
	      NOTE: abandoned the vpp work chaining concept, too high-level, cost
- [x] better textures: use optimal layout, new context-based uploading cmdbuf
- [x] rvg: reorganize/split header/sources
  - [x] shapes/context/texture/transform/scissor headers
  - [x] separate path.hpp in separate library/utility
  - ~~ [ ] also make nk/font.h public header ~~ (bad idea)
  - [x] split sources (state/context/shapes/text/font)
- [x] vui: pane
- [x] vui: improve/fix hacked together ColorButton
- [x] tooltip (note: implement as vui::(Delayed)Hint)
- [x] FIX! rvg: currently undefined behaviour when destroying an objects at
	certain points: after updateDevice (cmd buf recording) and before
	the cmd buf is executed.
- [x] think about dynamic scissor, avoiding rerecording on Widget::bounds

### v0.1

- [x] basic documentation (like at least one page stating basic usage)
  - [x] also check that there are at enough inline docs
  - [x] proof read
- [x] rvg: correct vulkan synchronization
  - [x] probably best to require the user to set it in the render pass or
        otherwise handle it. Document this somewhere
- [x] rvg::Context: use vpps new pipeline creation info (?)
- [x] srgb colors
	- [x] fix color mixing
- [x] clean up the DrawInstance mess
      or document why it is implemented that way.
	  We could also call it rvg::Context::bindDefaults(cmdBuf) which
	  matches it better. And change it if we really need something
	  like a DrawInstance state
	  [ended up deprecatng/removing DrawInstance. Not needed (atm)]
- [x] combine stageUpload and updateDevice (if possible).
- [x] Context::updateDevice return semantics when device objects that are not
      currently used are updated or new ones are created
	  [it's sane to rerecord even if objects currently not used and not
	   our business to handle new/destroyed device objects]
	- [x] also ctx.updateDevice() to return true when something was destroyed?
		  probably not, right?
- [x] stable (update) deps
	- [x] nytl
	- [x] vpp
	- [x] katachi
	- [x] dlg
	- [x] put their minimum versions in meson
- [x] basic demos with screenshots
- [x] split rvg and vui library
- [x] create real readme
- [x] release public version

