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
  'gtk_util.c',
]

foreach size : ['48', '64', '128', '256', '512']
  install_data('assets/scaled_' + size + 'x' + size + '.png', install_dir: get_option('datadir'), rename: 'icons/hicolor/' + size + 'x' + size + '/apps/io.github.lassebq.microlauncher.png')
endforeach
install_data('io.github.lassebq.microlauncher.desktop', install_dir: get_option('datadir') / 'applications')

m_dep = cc.find_library('m', required : false)

deps = [
  dependency('gtk4', version: '>= 4.0'),
  dependency('gio-2.0'),
  dependency('glib-2.0'),
  dependency('gobject-2.0'),
  dependency('libcurl'),
  dependency('libcrypto'),
  # dependency('uuid'),
  dependency('json-c'),
  dependency('libzip'),
  cc.find_library('ssp', static: true),
  m_dep
]
foreach dep : deps 

endforeach
exe = executable('microlauncher', sources,
  dependencies: deps,
  c_args: ['-fno-stack-protector'],
  # include_directories: include_directories,
  install : true
)

test('basic', exe)
