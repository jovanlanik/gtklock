# gtklock
# Copyright (c) 2023 Jianhua Lu, Jovan Lanik

wayland_scanner = find_program('wayland-scanner')

gen_wayland_protocol_c = generator(
  wayland_scanner,
  output : '@BASENAME@-protocol.c',
  arguments : ['private-code', '@INPUT@', '@OUTPUT@'],
)

gen_wayland_protocol_h = generator(
  wayland_scanner,
  output : '@BASENAME@-client-protocol.h',
  arguments : ['client-header', '@INPUT@', '@OUTPUT@'],
)

wayland_protocol_xmls = [
  'wlr-input-inhibitor-unstable-v1.xml',
]

wayland_sources = [
  gen_wayland_protocol_c.process(wayland_protocol_xmls),
  gen_wayland_protocol_h.process(wayland_protocol_xmls),
]
