#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform Paint {
	vec4 color;
} paint;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main() {
	out_color = paint.color * texture(tex, in_uv);
}
