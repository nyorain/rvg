example_args = []
if build_machine.system() == 'windows'
	example_args += '-DRVG_EXAMPLE_WIN'
elif build_machine.system() == 'linux'
	example_args += '-DRVG_EXAMPLE_UNIX'
endif

example_src = [
	'render.cpp',
]

example_deps = [
	rvg_dep,
]


# ny
if build_example_ny
  dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])
  example_src_ny = example_src + [
	  'example_ny.cpp',
	  'window.cpp',
  ]
  example_deps_ny = example_deps + [dep_ny]

  executable('example_ny',
	  cpp_args: example_args,
	  sources: example_src_ny,
	  dependencies: example_deps_ny)
endif


# glfw
if build_example_glfw
  dep_glfw = dependency('glfw3')
  example_src_glfw = example_src + ['example_glfw.cpp']
  example_deps_glfw = example_deps + [dep_glfw]

  executable('example_glfw',
	  cpp_args: example_args,
	  sources: example_src_glfw,
	  dependencies: example_deps_glfw)

  executable('scale',
	  cpp_args: example_args,
	  sources: example_src + ['scale.cpp'],
	  dependencies: example_deps_glfw)
endif

