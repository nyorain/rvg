dep_bugged = dependency('bugged', fallback: ['bugged', 'bugged_dep'])
test_deps = [
	rvg_dep,
	dep_bugged,
]

tests = [
	'context',
	'color',
	'render',
]

foreach test_name : tests
	exe = executable('test_' + test_name,
		sources: test_name + '.cpp',
		dependencies: test_deps)
	test(test_name, exe)
endforeach

