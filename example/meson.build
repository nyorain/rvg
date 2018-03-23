dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])

example_src = [
	'example.cpp',

	# 'gui.cpp',

	'render.cpp',
	'window.cpp']

example_deps = [
	vgv_dep,
	dep_ny]

executable('example', example_src,
	dependencies: example_deps)
