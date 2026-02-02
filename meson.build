project('asus_armoury', ['cpp', 'c'],
  version: '0.1.0',
  default_options: ['cpp_std=c++23']
)

gtk_dep = dependency('gtk4')
libadwaita_dep = dependency('libadwaita-1')

inc = include_directories('.')

shared_library('asus_armoury',
  [
    'asus_plugin.cpp',
    'backend/config.cpp',
    'backend/sysfs_writer.cpp',
    'backend/modes.cpp',
    'backend/core.cpp',
    'backend/battery.cpp',
    'backend/display.cpp',
    'backend/stress.cpp',
    'backend/monitor.cpp',
    'backend/brightness.cpp',
    'backend/keyboard.cpp',
    'backend/gpu.cpp',
    'backend/gpu_mux.cpp',
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
