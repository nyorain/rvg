#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out vec2 out_uv;

// layout(push_constant) uniform View {
// 	vec2 size;
// } view;

void main() {
	// gl_Position = vec4(2.0 * in_pos / view.size - 1.0, 0.0, 1.0);
	gl_Position = vec4(2.0 * in_pos - 1.0, 0.0, 1.0);
	out_uv = in_uv;
	out_pos = in_pos;
}
