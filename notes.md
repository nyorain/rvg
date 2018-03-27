
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
