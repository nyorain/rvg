#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec2 out_paint;
layout(location = 2) out vec4 out_color;

layout(row_major, set = 0, binding = 0) uniform Transform {
	mat4 matrix;
} transform;

layout(row_major, set = 1, binding = 0) uniform Paint {
	mat4 matrix;
} paint;

#if defined(plane_scissor)
	out float gl_ClipDistance[4];

	layout(set = 3, binding = 0) uniform Scissor {
		vec2 pos;
		vec2 size;
	} scissor;

	vec2 point(vec2 rpos, vec2 rsize, uint id) {
		vec2 ret = rpos;
		ret.x += float(id == 1 || id == 2) * rsize.x;
		ret.y += float(id == 2 || id == 3) * rsize.y;
		return ret;
	}

	void applyScissor() {
		uint last = 3;
		for(int i = 0; i < 4; ++i) {
			const vec2 p = point(scissor.pos, scissor.size, i);
			const vec2 diff = point(scissor.pos, scissor.size, last) - p;
			const vec2 normal = normalize(vec2(diff.y, -diff.x));
			gl_ClipDistance[i] = dot(in_pos, normal) - dot(p, normal);
			last = i;
		}
	}
#elif defined(frag_scissor)
	layout(location = 3) out vec2 out_rawpos;

	void applyScissor() {
		out_rawpos = in_pos;
	}
#else
	void applyScissor() {}
#endif

void main() {
	gl_Position = transform.matrix * vec4(in_pos, 0.0, 1.0);
	out_paint = (paint.matrix * vec4(in_pos, 0.0, 1.0)).xy;
	out_uv = in_uv;

	out_color = in_color;
	applyScissor();
}
