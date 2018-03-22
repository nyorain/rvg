#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform Paint {
	vec4 color;
} paint;

layout(push_constant) uniform Push {
	float strokeMult;
} push;

layout(set = 1, binding = 0) uniform sampler2D tex;

float strokeMask() {
	return min(1.0, (1.0 - abs(in_uv.y)) * push.strokeMult);
}

void main() {
	out_color = strokeMask() * paint.color * texture(tex, in_uv);
}
