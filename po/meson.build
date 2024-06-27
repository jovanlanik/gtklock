# gtklock
# Copyright (c) 2023 Kenny Levinsen, Jovan Lanik

if import('fs').is_absolute(get_option('localedir'))
  localedir = get_option('localedir')
else
  localedir = get_option('prefix') / get_option('localedir')
endif

add_project_arguments(
  '-DGETTEXT_PACKAGE="' + meson.project_name() + '"', 
  '-DLOCALEDIR="' + localedir + '"',
  language:'c'
)

i18n = import('i18n')

i18n.gettext(meson.project_name(),
    args: '--directory=' + meson.source_root(),
    preset: 'glib'
)
