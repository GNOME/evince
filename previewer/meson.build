previewer_sources = files(
  'ev-previewer-window.c',
  'ev-previewer-window.h',
  'ev-previewer.c',
)

previewer_sources += gnome.compile_resources(
  'ev-previewer-resources',
  'previewer.gresource.xml',
  source_dir: data_dir,
  c_name: 'ev_previewer',
)

previewer_deps = [
  libevdocument_dep,
  libevmisc_dep,
  libevview_dep,
  libaw_dep
]

if enable_gtk_unix_print
  previewer_deps += gtk_unix_print_dep
endif

previewer = executable(
  'evince-previewer',
  previewer_sources,
  include_directories: top_inc,
  dependencies: previewer_deps,
  link_args: common_ldflags,
  install: true,
)
