# gtklock
# Copyright (c) 2023 Jianhua Lu, Jovan Lanik

project(
  'gtklock', 'c',
  version : '3.0.0',
  license : 'GPLv3',
  default_options : ['c_std=c11']
)

version = meson.project_version().split('.')
major_version = version[0]
minor_version = version[1]
micro_version = version[2]

gtk = dependency('gtk+-3.0')
gtk_session_lock = dependency('gtk-session-lock-0')
gmodule_export = dependency('gmodule-export-2.0')
pam = dependency('pam')

dependencies = [
  gtk,
  gtk_session_lock,
  gmodule_export,
  pam,
]

subdir('man')
subdir('res')
subdir('src')

gtklock_set = [
  gtklock_sources,
  ui_resources,
]

executable(
  meson.project_name(),
  gtklock_set,
  include_directories : 'include',
  dependencies : dependencies,
  c_args : [
    '-DPREFIX=' + get_option('prefix'),
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
