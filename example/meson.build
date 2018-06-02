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

example_args = []

if build_machine.system() == 'windows'
	example_args += '-DRVG_EXAMPLE_WIN'
elif build_machine.system() == 'linux'
	example_args += '-DRVG_EXAMPLE_UNIX'
endif

executable('example',
	cpp_args: example_args,
	sources: example_src,
	dependencies: example_deps)
