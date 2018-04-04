shaders_dep = files('paint.glsl')
shaders_src = [
	'fill.vert',
	'fill.frag',
]

shader_configs = [
	['.plane_scissor', '-DPLANE_SCISSOR'],
	['.frag_scissor', '-DFRAG_SCISSOR'],
	['.plane_scissor.edge_aa', ['-DPLANCE_SCISSOR', '-DEDGE_AA']],
	['.frag_scissor.edge_aa', ['-DFRAG_SCISSOR', '-DEDGE_AA']],
]

shaders = []
glslang = find_program('glslangValidator')

foreach config : shader_configs
	foreach shader : shaders_src
		name = shader.underscorify() + config[0].underscorify() + '_data'
		args = [glslang, '-V', '@INPUT@', '-o', '@OUTPUT@', '--vn', name]
		args += config[1]
		header = custom_target(
			shader + config[0] + '_spv',
			output: shader + config[0] + '.h',
			input: shader,
			depend_files: shaders_dep,
			command: args)

		shaders += [header]
	endforeach
endforeach
