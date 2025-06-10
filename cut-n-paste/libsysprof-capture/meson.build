libsysprof_capture_headers = files(
  'sysprof-address.h',
  'sysprof-clock.h',
  'sysprof-capture-condition.h',
  'sysprof-capture-cursor.h',
  'sysprof-capture-reader.h',
  'sysprof-capture-types.h',
  'sysprof-capture-writer.h',
  'sysprof-collector.h',
  'sysprof-macros.h',
  'sysprof-platform.h',
  'sysprof-capture.h',
  'sysprof-version-macros.h',
)

mapped_ring_buffer_sources = files(
  'mapped-ring-buffer.c',
)

libsysprof_capture_sources = files(
  'sysprof-address.c',
  'sysprof-capture-condition.c',
  'sysprof-capture-cursor.c',
  'sysprof-capture-reader.c',
  'sysprof-capture-util.c',
  'sysprof-capture-writer.c',
  'sysprof-capture-writer-cat.c',
  'sysprof-collector.c',
  'sysprof-clock.c',
  'sysprof-platform.c',
)

libsysprof_capture_deps = [
  dependency('threads'),
]

libsysprof_capture = static_library(
  'sysprof-capture',
  sources: libsysprof_capture_sources + mapped_ring_buffer_sources,
  include_directories: top_inc,
  dependencies: libsysprof_capture_deps,
  c_args: [ '-DSYSPROF_CAPTURE_COMPILATION' ],
  gnu_symbol_visibility: 'hidden',
  pic: true,
)

libsysprof_capture_include_dirs = include_directories('.')

libsysprof_capture_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: libsysprof_capture_deps,
  link_with: libsysprof_capture,
)
