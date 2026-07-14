project('asus_armoury', ['cpp', 'c'],
  version: '0.1.0',
  default_options: ['cpp_std=c++23']
)

add_project_arguments('-DAAC_SOURCE_DIR="' + meson.project_source_root() + '"', language: 'cpp')

gtk_dep = dependency('gtk4')
libadwaita_dep = dependency('libadwaita-1')
glib_dep = dependency('glib-2.0')

inc = include_directories('.')

backend_sources = [
  'backend/config.cpp',
  'backend/sysfs_writer.cpp',
  'backend/modes.cpp',
  'backend/battery.cpp',
  'backend/gpu.cpp',
  'backend/gpu_mux.cpp',
  'backend/keyboard.cpp',
  'backend/keybind.cpp',
]

shared_library('asus_armoury',
  [
    'asus_plugin.cpp',
    backend_sources,
    'backend/core.cpp',
    'backend/display.cpp',
    'backend/stress.cpp',
    'backend/monitor.cpp',
    'backend/brightness.cpp',
    'backend/visuals.cpp',
    'backend/fan_control.cpp',
    'ui/notification.cpp',
  ],
  dependencies: [gtk_dep, libadwaita_dep],
  include_directories: inc,
  install: true,
  install_dir: get_option('libdir') / 'kerneldrive/plugins',
  name_prefix: ''
)

executable('AAC',
  [
    'aac.cpp',
    backend_sources,
  ],
  dependencies: [glib_dep],
  include_directories: inc,
  install: true,
  install_dir: get_option('bindir'),
)
