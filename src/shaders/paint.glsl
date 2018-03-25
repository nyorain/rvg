struct PaintData {
	vec4 inner;
	vec4 outer;
	vec2 extent;
	float radius;
	float feather;
	uint type;
};

float sdroundrect(vec2 coords, vec2 ext, float radius) {
	vec2 d = abs(coords) - ext + vec2(radius, radius);
	return min(max(d.x,d.y), 0.0) + length(max(d, 0.0)) - radius;
}

const uint paintTypeColor = 1u;
const uint paintTypeGradient = 2u;
const uint paintTypeTexRGBA = 3u;
const uint paintTypeTexA = 4u;

vec4 paintColor(vec2 coords, PaintData paint, sampler2D tex) {
	if(paint.type == paintTypeColor) {
		return paint.inner;
	} else if(paint.type == paintTypeGradient) {
		float sdr = sdroundrect(coords, paint.extent, paint.radius);
		float d = (sdr + paint.feather * 0.5) / paint.feather;
		return mix(paint.inner, paint.outer, clamp(d, 0, 1));
	} else if(paint.type == paintTypeTexRGBA) {
		return paint.inner * texture(tex, coords);
	} else if(paint.type == paintTypeTexA) {
		return paint.inner * texture(tex, coords).a;
	}

	return vec4(1, 1, 1, 1);
}
