# Retained vulkan/vector graphics

Vulkan library for high-level 2D vector-like rendering.
Modeled loosely after svg, inspired by nanoVG.
Implements retained mode for rendering which makes it highly efficient
for rendering with vulkan since curves and shapes are not recomputed
and uploaded every frame but just once (or when changed).
Does not even need a command buffer rerecord every frame,
even things like paints, shapes or transforms can be
changed without triggering the need for a command buffer rerecord which makes
it use way less cpu performance than immediate mode alternatives.
Could e.g. easily be used for a gui library.

For some information check out the example concepts below, the real
heavily documented [example](example/example.cpp) showing off most features
or read the [introduction](docs/Intro.md).

# Example

<TODO>

## Screenshots

<TODO>

# Notes

Please read the introduction (requirements) before reporting build issues.
You need a solid C++17 compiler (MSVC won't work at the moment since it
does not support all needed features correctly).

Contributions welcome, always want to hear ideas and suggestions.

This library is the successor of/inspired my attempt to write a 
[nanoVG vulkan backend](https://github.com/nyorain/vvg), which worked
but didn't really make use of vulkans advantages. That inspired
me to look into which kind of abstraction would make sense for vulkan.
