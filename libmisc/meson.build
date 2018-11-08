sources = files(
  'ev-page-action-widget.c',
  'ev-search-box.c',
)

libevmisc = static_library(
  'evmisc',
  sources: sources,
  include_directories: top_inc,
  dependencies: libevview_dep,
  c_args: '-DEVINCEDATADIR="@0@"'.format(ev_pkgdatadir),
)

libevmisc_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: libevview_dep,
  link_whole: libevmisc,
)
