# Retained vulkan/vector graphics

Vulkan library for high-level 2D vector-like rendering.
Modeled loosely after svg, inspired by nanoVG.
Uses retained mode for rendering which makes it highly efficient
for rendering with vulkan since curves and shapes are not recomputed
and uploaded every frame but just once. Does not even need a command buffer
rerecord every frame, even things like paints, shapes or transforms can be
changed without triggering the need for a command buffer rerecord which makes
it use way less cpu performance than immediate mode alternatives.
Could easily be used for a gui library.

## Roadmap

- [x] windows (basic panes, no operations)
- [x] widget sizing options, Window::create auto sizing
  - [x] auto sizing on construction
- [x] row layouts
- [x] slider
- [x] rvg: rounded rect
- [ ] more advanced textfield
  - [x] scrolling, clipping
  - [ ] enter/escape
  - [x] selection
  - [ ] some basic shortcuts like ctrl-a (might need ny fixes)
- [x] fix widget resizing todos
- [x] rvg: sanity checking (with asserts/logs)
  - [x] color functions, conversion
  - [x] shape drawing functions
- [x] rvg: antialiasing
  - [x] fix stroke caps
  - [x] better settings (Context, DrawMode)
  - [x] simplify Polygon
- [ ] rvg: paint/transform/scissor on deviceLocal memory
  - [ ] probably enough when specifiable on construction, must not be dynamic
  - [ ] also text
  - [x] more efficient staging writes. Don't submit command buffer at once
    - [x] use semaphores (-> vpp: work chaining)
	      NOTE: abandoned the vpp work chaining concept, too high-level, cost
- [x] rvg: correct vulkan synchronization
  - [ ] probably best to require the user to set it in the render pass or
        otherwise handle it. Document this somewhere
- [ ] better textures: use optimal layout, new context-based uploading cmdbuf
- [ ] rvg: reorganize/split header/sources
  - [x] shapes/context/texture/transform/scissor headers
  - [x] separate path.hpp in separate library/utility
  - ~~ [ ] also make nk/font.h public header ~~ (bad idea)
  - [ ] split sources (state/context/shapes/text/font)
- [ ] rvg: make non-texture gradients make use of transform buffer span
- [ ] nanovg like box gradient
- [ ] readd vui::Slider (with (optional?) different style)
- [ ] implement vui cursor image callback in gui listener (e.g. textfield hover)
- [ ] split rvg and vui library
- [ ] clipboard support (probably over Gui/GuiListener)
- [ ] vui: horizontal splitting line
- [ ] vui: radio button
- [ ] don't use that much paints and descriptors for widgets
  -> optional dynamic new ext descriptor support
  -> advanced styling/themes
- [ ] allow to specify fringe on context creation (?)
- [ ] widget styles, also spacings/paddings/margins/borders etc
- [ ] popups (needed for dropdown menu, tooltip)
- [x] z widget ordering
  - [ ] temporary raise on one layer (reorder in vector)
  - [ ] allow widgets to change it? needed?
- [ ] dropdown menu
- [ ] tooltip
- [ ] tabs (vui::TabbedPane or something as class)
- [ ] better mouse/keyboard grabs
  - [ ] currently bugs when multiple button grabs
  - [ ] key grabs (needed?)
- [x] think about dynamic scissor, avoiding rerecording on Widget::bounds
- [ ] benchmark alternative pipelines, optimize default use cases
  - [ ] performance optimizations, resolve performance todos
  - [ ] rvg: better with more (but also more optimized) pipelines?
- [ ] beautiful demos with screenshots
- [ ] multistop gradients (?), using small 1d textures
  - [ ] see discussion https://github.com/memononen/nanovg/pull/430
- [ ] release public version
- [ ] rvg: more stroke settings: linecap/linejoin [complex; -> katachi]
- [ ] window decorations/integrate with tabs
- [ ] animations
- [ ] advanced widget sizing hints, min/max size (needed?)
- [ ] textfields/slider combos for ints/floats
- [ ] better,easier custom navigation (e.g. tab-based)
- [ ] custom grabbing slider
- [ ] window operations (move, resize) (?)
- [ ] graph widget, e.g. for frametimes
- [ ] drag and drop stuff (not sure if needed at all)
- [ ] helper for non-convex shapes (in rvg: stencil buffer? or decomposition?)
