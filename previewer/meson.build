sources = files(
  'ev-previewer.c',
  'ev-previewer-toolbar.c',
  'ev-previewer-window.c',
)

resource_data = files(
  '../data/thumbnail-frame.png',
  'evince-previewer.css',
  'previewer.ui',
)

sources += gnome.compile_resources(
  'ev-previewer-resources',
  'previewer.gresource.xml',
  source_dir: data_dir,
  c_name: 'ev_previewer',
  dependencies: resource_data,
  export: true,
)

deps = [libevmisc_dep]

if enable_gtk_unix_print
  deps += gtk_unix_print_dep
endif

program = 'evince-previewer'

executable(
  program,
  sources,
  include_directories: top_inc,
  dependencies: deps,
  link_args: common_ldflags,
  install: true,
)
