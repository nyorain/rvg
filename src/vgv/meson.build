dep_m = meson.get_compiler('c').find_library('m')

vgv_src = [
	'vgv.cpp', 
	shaders
]

vgv_deps = [dep_m, dep_vpp]

vgv_lib = library('vgv', 
	sources: vgv_src, 
	dependencies: vgv_deps,
	include_directories: [src_inc, vgv_inc])
