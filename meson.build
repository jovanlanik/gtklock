# gtklock
# Copyright (c) 2023 Jianhua Lu, Jovan Lanik, Zephyr Lykos

project(
  'gtklock', 'c',
  version : '4.0.0',
  license : 'GPLv3',
  default_options : ['c_std=c11']
)

version = meson.project_version().split('.')
major_version = version[0]
minor_version = version[1]
micro_version = version[2]

gtk = dependency('gtk4')
gtk_session_lock = dependency('gtk4-layer-shell')
gmodule_export = dependency('gmodule-export-2.0')
pam = meson.get_compiler('c').find_library('pam')

dependencies = [
  gtk,
  gtk_session_lock,
  gmodule_export,
  pam,
]

subdir('po')
subdir('man')
subdir('res')
subdir('src')

gtklock_set = [
  gtklock_sources,
  ui_resources,
]

if import('fs').is_absolute(get_option('libdir'))
  libdir = get_option('libdir')
else
  libdir = get_option('prefix') / get_option('libdir')
endif

executable(
  meson.project_name(),
  gtklock_set,
  include_directories : 'include',
  dependencies : dependencies,
  c_args : [
    '-DLIBDIR=' + libdir,
    '-DMAJOR_VERSION=' + major_version,
    '-DMINOR_VERSION=' + minor_version,
    '-DMICRO_VERSION=' + micro_version,
  ],
  install : true
)

install_data(
  'pam/gtklock',
  install_dir : get_option('sysconfdir') / 'pam.d'
)
