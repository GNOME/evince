sources = files(
  'dzl-file-manager.c',
)

deps = [
  glib_dep,
]

libdazzle = static_library(
  'dazzle',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
)

libdazzle_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: deps,
  link_with: libdazzle,
)
