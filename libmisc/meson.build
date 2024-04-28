sources = files(
  'ev-page-action-widget.c',
)

sources += gnome.compile_resources(
  'ev-resources',
  'libmisc.gresource.xml',
  c_name: 'libmisc',
)

libevmisc = static_library(
  'evmisc',
  sources: sources,
  include_directories: top_inc,
  dependencies: libevview_dep,
)

libevmisc_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: libevview_dep,
  link_whole: libevmisc,
)
