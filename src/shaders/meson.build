shaders_dep = files('paint.glsl')
shaders_src = [
	'fill.vert',
	'fill.frag',
]

shaders = []
glslang = find_program('glslangValidator')

foreach shader : shaders_src
	name = shader.underscorify() + '_data'
	header = custom_target(
		shader + '_spv',
		output: shader + '.h',
		input: shader,
		depend_files: shaders_dep,
		command: [glslang, '-V', '@INPUT@', '-o', '@OUTPUT@', '--vn', name])

	shaders += [header]
endforeach
