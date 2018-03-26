struct PaintData {
	vec4 inner;
	vec4 outer;
	vec4 custom;
	uint type;
};

const uint paintTypeColor = 1u;
const uint paintTypeLinGrad = 2u;
const uint paintTypeRadGrad = 3u;
const uint paintTypeTexRGBA = 4u;
const uint paintTypeTexA = 5u;
const uint paintTypePointColor = 6u;

vec4 paintColor(vec2 coords, PaintData paint, sampler2D tex, vec4 col) {
	if(paint.type == paintTypeColor) {
		return paint.inner;
	} else if(paint.type == paintTypeLinGrad) {
		vec2 start = paint.custom.xy;
		vec2 end = paint.custom.zw;
		vec2 dir = end - start;
		float fac = dot(coords - start, dir) / dot(dir, dir);
		return mix(paint.inner, paint.outer, clamp(fac, 0, 1));
	} else if(paint.type == paintTypeRadGrad) {
		vec2 center = paint.custom.xy;
		float r1 = paint.custom.z;
		float r2 = paint.custom.w;
		float fac = (length(coords - center) - r1) / (r2 - r1);
		return mix(paint.inner, paint.outer, clamp(fac, 0, 1));
	} else if(paint.type == paintTypeTexRGBA) {
		return paint.inner * texture(tex, coords);
	} else if(paint.type == paintTypeTexA) {
		return paint.inner * texture(tex, coords).a;
	} else if(paint.type == paintTypePointColor) {
		return col;
	}

	return vec4(1, 1, 1, 1);
}
