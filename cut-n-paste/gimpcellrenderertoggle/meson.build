libgimpcellrenderertoggle = static_library(
  'gimpcellrenderertoggle',
  sources: 'gimpcellrenderertoggle.c',
  include_directories: top_inc,
  dependencies: gtk_dep,
)

libgimpcellrenderertoggle_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtk_dep,
  link_with: libgimpcellrenderertoggle,
)
