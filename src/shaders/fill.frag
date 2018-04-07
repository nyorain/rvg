#version 450

#extension GL_GOOGLE_include_directive : enable
#include "paint.glsl"

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec2 in_paint;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 1) uniform Paint {
	PaintData data;
} paint;

layout(set = 1, binding = 2) uniform sampler2D tex;
layout(set = 2, binding = 0) uniform sampler2D font;

const uint TypeDefault = 0;
const uint TypeText = 1;
const uint TypeStroke = 2;
layout(push_constant) uniform Type {
	uint type;
} type;

// - scissor -
#ifdef FRAG_SCISSOR
	layout(location = 3) in vec2 in_rawpos;

	layout(set = 3, binding = 0) uniform Scissor {
		vec2 pos;
		vec2 size;
	} scissor;

	void applyScissor() {
		vec2 c = clamp(in_rawpos, scissor.pos, scissor.pos + scissor.size);
		if(in_rawpos != c) {
			discard;
		}
	}
#else // FRAG_SCISSOR
	void applyScissor() {}
#endif

// - anti aliasing -
#ifdef EDGE_AA
	layout(set = 4, binding = 0) uniform Stroke {
		float mult;
	} stroke;
#endif

// - main -
void main() {
	applyScissor();
	out_color = paintColor(in_paint, PaintData(
		paint.data.inner,
		paint.data.outer,
		paint.data.custom,
		paint.data.type), tex, in_color);

	if(type.type == TypeText) {
		out_color.a *= texture(font, in_uv).a;
	}

#ifdef EDGE_AA
	if(type.type == TypeStroke) {
		// float fac = (1.0 - abs(in_uv.y)) * stroke.mult * in_uv.x;
		float fac = (min(1.0, 1.0 - abs(in_uv.y)) * stroke.mult) * in_uv.x;
		out_color.a *= fac;
	}
#endif
}
