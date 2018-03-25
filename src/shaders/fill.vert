#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec2 out_paint;

layout(row_major, set = 0, binding = 0) uniform Transform {
	mat4 matrix;
} transform;

layout(row_major, set = 1, binding = 0) uniform Paint {
	mat4 matrix;
} paint;

void main() {
	gl_Position = transform.matrix * vec4(in_pos, 0.0, 1.0);
	out_paint = (transform.matrix * paint.matrix * vec4(in_pos, 0.0, 1.0)).xy;
	// out_paint = (paint.matrix * vec4(in_pos, 0.0, 1.0)).xy;
	out_uv = in_uv;
}
