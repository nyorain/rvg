dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])

example_src = [
	'example.cpp',
	'render.cpp',
	'window.cpp',

	'vui/gui.cpp',
	'vui/button.cpp',
	'vui/widget.cpp',
	'vui/hint.cpp',
	'vui/container.cpp',
	'vui/window.cpp',
	'vui/colorPicker.cpp',
	'vui/textfield.cpp',
]

example_deps = [
	rvg_dep,
	dep_ny
]

executable('example', example_src,
	dependencies: example_deps)
