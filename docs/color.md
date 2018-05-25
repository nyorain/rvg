About Color and color Spaces
============================

Great resources and discussions regarding color spaces and gamma correction:
- http://blog.johnnovak.net/2016/09/21/what-every-coder-should-know-about-gamma/
- https://github.com/ocornut/imgui/issues/578


*Issue*: should the colors passed to paints be in rgb or srgb color space?

- the web specifies color in srgb (html, css) by default
- otherwise (rgb) you might have to create colors like (1, 1, 1) and (2, 2, 2)
  for dark colors; you will really start to notice that you have
  a limited amount of dark colors
  	- we don't want to use floating point colors (as vertex input at least)
- when specifying color (128, 128, 128) i would expect the screen to have
  half brightness. But (128, 128, 128) in rgb isn't half brightness but much
  too bright.
- when specifying colors in srgb we have to convert them back to rgb every
  time for operations (e.g. for gradients)
  	- when should we do that? in the shader seems a bad idea, should probably
	  be done by the rvg::Paint class

*Resolved*: colors given to rvg are in srgb space. Paint class will convert it
  to (linear) rgb before uploading it on the device. rvg offers functionality
  for converting (with gamma) and also mixing colors.



*Issue*: should rvg offer a gamma setting for its shader? This would be useful
when rendering on a not-srgb framebuffer.

- we could use a push constant (having almost no cost) since it will
  usually not change all the time
- otherwise vulkan supports srgb, there is practially no reason to not use it

*Resolved*: since it is usually not done, rvg will not do it for now
