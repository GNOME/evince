sources = files(
  'gd-icon-utils.c',
  'gd-two-lines-renderer.c',
)

deps = [
  cairo_dep,
  gtk_dep,
]

libgd = static_library(
  'gd',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
)

libgd_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: deps,
  link_with: libgd,
)
