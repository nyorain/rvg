project('vgv', ['cpp', 'c'],
	license: 'MIT',
	version: '0.1.0',
	meson_version: '>=0.43',
	default_options: [
		'cpp_std=c++1z',
		'c_std=c99',
  		'warning_level=3',
  		'werror=true'])

# TODO: settings
build_example = true

warnings = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',

	# extra
	'-Wduplicated-cond',
	'-Wrestrict',
	'-Wnull-dereference',
	'-Wundef',
	'-Wcast-align=strict',
	'-Wlogical-op',

	# disable some
	'-Wno-non-virtual-dtor'
]

# default arrguments
# warnings and stuff
cc = meson.get_compiler('cpp')
add_project_arguments(
	cc.get_supported_arguments(warnings),
	language: 'cpp')

# project-specific stuff
# source_root = meson.source_root().split('\\')
# add_project_arguments('-DDLG_BASE_PATH="' + '/'.join(source_root) + '/"', language: 'cpp')

dep_vpp = dependency('vpp', fallback: ['vpp', 'vpp_dep'])
dep_vulkan = dependency('vulkan')

src_inc = include_directories('src') # for shaders, internal headers
vgv_inc = include_directories('include')

subdir('src/shaders')
subdir('src/vgv')

dep_vgv = declare_dependency(
	link_with: vgv_lib,
	include_directories: vgv_inc,
	dependencies: dep_vpp)

if build_example
  subdir('example')
endif
