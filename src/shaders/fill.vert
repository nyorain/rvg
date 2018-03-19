#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_pos;
layout(location = 1) out vec2 out_uv;

layout(row_major, set = 2, binding = 0) uniform Transform {
	mat4 matrix;
} transform;

void main() {
	gl_Position = transform.matrix * vec4(in_pos, 0.0, 1.0);
	out_uv = in_uv;
	out_pos = in_pos;
}
