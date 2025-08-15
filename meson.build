project('microlauncher', 'c',
  version : '0.1',
  default_options : ['warning_level=2'])

add_project_arguments(['-Wno-unused-parameter', '-Wno-cast-function-type'], language: 'c')
add_project_arguments('-DMICROSOFT_CLIENT_ID="' + get_option('client_id') + '"', language: 'c')

cc = meson.get_compiler('c')

sources = [
  'microlauncher.c',
  'microlauncher_msa.c',
  'microlauncher_gui.c',
  'microlauncher_version_item.c',
  'xdgutil.c',
  'util.c',
  'json_util.c',
  'gtk_util.c'
]

m_dep = cc.find_library('m', required : false)

deps = [
  dependency('gtk4', version: '>= 4.0'),
  dependency('libcurl'),
  dependency('libcrypto'),
  dependency('uuid'),
  dependency('json-c'),
  dependency('libzip'),
  m_dep
]

exe = executable('microlauncher', sources,
  dependencies: deps,
  install : true
)

test('basic', exe)
