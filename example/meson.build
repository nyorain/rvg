dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])

example_src = [
	'example.cpp',
	'render.cpp',
	'window.cpp']

example_deps = [
	dep_vgv,
	dep_ny,
	dep_vpp,
	dep_vulkan]

executable('example', example_src, 
	dependencies: example_deps)
