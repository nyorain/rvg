vgv_src = [
	'vgv.cpp',
	'texture.cpp',
	'nk_font/font.c',
	shaders
]

vgv_deps = [dep_vpp, meson.get_compiler('c').find_library('m')]

vgv_lib = library('vgv',
	sources: vgv_src,
	dependencies: vgv_deps,
	include_directories: [src_inc, vgv_inc])
