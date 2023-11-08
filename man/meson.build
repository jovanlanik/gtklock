# gtklock
# Copyright (c) 2023 Jianhua Lu, Jovan Lanik

scdoc = find_program('scdoc', required: get_option('man-pages'))

if scdoc.found()
  mandir = get_option('mandir')
  man_files = [
    'gtklock.1.scd',
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
