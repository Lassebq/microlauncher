project('microlauncher', 'c',
  version : '0.1',
  default_options : ['warning_level=2'])

sources = [
  'microlauncher.c',
  'microlauncher_gui.c',
  'xdgutil.c',
  'util.c'
]

deps = [
  dependency('gtk4', version: '>= 4.0'),
  dependency('json-c'),
  dependency('libcurl')
]

exe = executable('microlauncher', sources,
  dependencies: deps,
  install : true)

test('basic', exe)
