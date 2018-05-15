dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])

example_src = [
	'example.cpp',
	'render.cpp',
	'window.cpp',
]

example_deps = [
	rvg_dep,
	dep_ny
]

executable('example',
	sources: example_src,
	dependencies: example_deps)
