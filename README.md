# Retained vulkan/vector graphics

Vulkan library for high-level 2D vector-like rendering.
Modeled loosely after svg, inspired by nanoVG.
Uses retained mode for rendering which makes it highly efficient
for rendering with vulkan. Could easily be used for a gui library.

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
  - [ ] fix stroke caps
  - [ ] better settings (Context, DrawMode)
  - [ ] simplify Polygon
- [ ] rvg: paint/transform/scissor on deviceLocal memory optionally
  - [ ] more efficient staging writes. Don't submit command buffer at once
- [ ] rvg: correct vulkan synchronization
- [ ] rvg: reorganize/split header/sources
  - [x] shapes/context/texture/transform/scissor headers
  - [x] separate path.hpp in separate library/utility
  - ~~ [ ] also make nk/font.h public header ~~ (bad idea)
- [ ] rvg: make non-texture gradients make use of transform buffer span
- [ ] split rvg and gui library
- [ ] clipboard support (probably over Gui/GuiListener)
- [ ] don't use that much paints and descriptors for widgets
  -> advanced styling/themes
- [ ] widget styles, also spacings/paddings/margins/borders etc
- [ ] popups (needed for dropdown menu, tooltip)
- [x] z widget ordering
  - [ ] temporary raise on one layer (reorder in vector)
  - [ ] allow widgets to change it? needed?
- [ ] dropdown menu
- [ ] tooltip
- [ ] tabs
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
- [ ] rvg: more stroke settings: linecap/linejoin
- [ ] animations
- [ ] advanced widget sizing hints, min/max size (needed?)
- [ ] textfields/slider combos for ints/floats
- [ ] better,easier custom navigation (e.g. tab-based)
- [ ] custom grabbing slider
- [ ] window operations
- [ ] window decorations/integrate with tabs
- [ ] graph widget, e.g. for frametimes
- [ ] drag and drop stuff (not sure if needed at all)
- [ ] helper for non-convex shapes (in rvg: stencil buffer? or decomposition?)
