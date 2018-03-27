#version 450

layout(location = 0) in vec2 vin_pos;
layout(location = 1) in vec2 vin_uv;
layout(location = 2) in vec4 vin_color;
/* layout(location = 2) in uint in_color; */

layout(location = 0) out vec2 vout_uv;
layout(location = 1) out vec2 vout_paint;
layout(location = 2) out vec4 vout_color;

layout(row_major, set = 0, binding = 0) uniform Transform {
	mat4 matrix;
} transform;

layout(row_major, set = 1, binding = 0) uniform Paint {
	mat4 matrix;
} paint;

void main() {
	gl_Position = transform.matrix * vec4(vin_pos, 0.0, 1.0);
	vout_paint = (paint.matrix * vec4(vin_pos, 0.0, 1.0)).xy;
	vout_uv = vin_uv;

	vout_color = vin_color;
}
