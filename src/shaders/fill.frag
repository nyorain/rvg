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

layout(push_constant) uniform Text {
	bool is;
} text;

void main() {
	out_color = paintColor(in_paint, PaintData(
		paint.data.inner,
		paint.data.outer,
		paint.data.custom,
		paint.data.type), tex, in_color);

	if(text.is) {
		out_color.a *= texture(font, in_uv).a;
	}
}
