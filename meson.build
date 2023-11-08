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
gtk_layer_shell = dependency('gtk-layer-shell-0')
gmodule_export = dependency('gmodule-export-2.0')
pam = dependency('pam')
wayland_client = dependency('wayland-client')

dependencies = [
  gtk,
  gtk_layer_shell,
  gmodule_export,
  pam,
  wayland_client,
]

scdoc = find_program('scdoc', required: get_option('man-pages'))

subdir('res')
subdir('src')
subdir('wayland')

gtklock_set = [
  gtklock_sources,
  uiresources,
  wayland_sources,
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

if scdoc.found()
  mandir = get_option('mandir')
  man_files = [
    'man/gtklock.1.scd',
  ]
  foreach filename : man_files
    topic = filename.split('.')[-3].split('/')[-1]
    section = filename.split('.')[-2]
    output = '@0@.@1@'.format(topic, section)

    custom_target(
      output,
      input: filename,
      output: output,
      command: scdoc,
      feed: true,
      capture: true,
      install: true,
      install_dir: '@0@/man@1@'.format(mandir, section)
    )
  endforeach
endif
