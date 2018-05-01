project('rvg', ['cpp', 'c'],
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
build_tests = true

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
	'-Wno-non-virtual-dtor',

	# TODO
	'-Wno-unused-function',
]

# default arrguments
# warnings and stuff
cc = meson.get_compiler('cpp')
add_project_arguments(
	cc.get_supported_arguments(warnings),
	language: 'cpp')

# project-specific stuff
source_root = meson.source_root().split('\\')
add_project_arguments('-DDLG_BASE_PATH="' + '/'.join(source_root) + '/"', language: 'cpp')

dep_vpp = dependency('vpp', fallback: ['vpp', 'vpp_dep'])
dep_nytl = dependency('nytl MASTER', fallback: ['nytl', 'nytl_dep'])
dep_dlg = dependency('dlg', fallback: ['dlg', 'dlg_dep'])
dep_katachi = dependency('katachi', fallback: ['katachi', 'katachi_dep'])
dep_vulkan = dependency('vulkan')

src_inc = include_directories('src') # for shaders, internal headers
rvg_inc = include_directories('include')

rvg_deps = [
  dep_vpp,
  dep_nytl,
  dep_dlg,
  dep_katachi,
  dep_vulkan
]

subdir('src/shaders')
subdir('src/rvg')

rvg_dep = declare_dependency(
	link_with: rvg_lib,
	dependencies: rvg_deps,
	include_directories: rvg_inc)

if build_example
  subdir('example')
endif

if build_tests
  subdir('docs/tests')
endif

# pkgconfig
pkg = import('pkgconfig')
pkg_dirs = ['.']
pkg.generate(
	name: 'rvg',
	requires: ['vulkan', 'vpp', 'nytl'],
	libraries: rvg_lib,
	filebase: 'rvg',
	subdirs: pkg_dirs,
	version: meson.project_version(),
	description: 'Retained vulkan/vector graphics')
