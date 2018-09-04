libview_inc = include_directories('.')

include_subdir = join_paths(ev_include_subdir, 'libview')

headers = files(
  'ev-document-model.h',
  'ev-jobs.h',
  'ev-job-scheduler.h',
  'ev-print-operation.h',
  'ev-stock-icons.h',
  'ev-view.h',
  'ev-view-presentation.h'
)

install_headers(
  headers,
  subdir: include_subdir,
)

sources = files(
  'ev-annotation-window.c',
  'ev-document-model.c',
  'ev-form-field-accessible.c',
  'ev-image-accessible.c',
  'ev-jobs.c',
  'ev-job-scheduler.c',
  'ev-link-accessible.c',
  'ev-page-accessible.c',
  'ev-page-cache.c',
  'ev-pixbuf-cache.c',
  'ev-print-operation.c',
  'ev-stock-icons.c',
  'ev-timeline.c',
  'ev-transition-animation.c',
  'ev-view.c',
  'ev-view-accessible.c',
  'ev-view-cursor.c',
  'ev-view-presentation.c',
)

marshal = 'ev-view-marshal'

marshal_sources = gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: 'ev_view_marshal',
  internal: true,
  extra_args: '--prototypes',
)

enum_types = 'ev-view-type-builtins'

enum_sources = gnome.mkenums(
  enum_types,
  sources: headers,
  h_template: enum_types + '.h.template',
  c_template: enum_types + '.c.template',
  install_header: true,
  install_dir: join_paths(ev_includedir, include_subdir),
)

deps = [
  gspell_dep,
  gstreamer_base_dep,
  gstreamer_dep,
  gstreamer_video_dep,
  gtk_unix_print_dep,
  gthread_dep,
  libevdocument_dep,
  m_dep,
]

cflags = [
  '-DG_LOG_DOMAIN="EvinceView"',
  '-DEVINCEDATADIR="@0@"'.format(ev_pkgdatadir),
  '-DEVINCE_COMPILATION'
]

if enable_multimedia
  sources += files('ev-media-player.c')
endif

libevview = shared_library(
  'evview' + ev_major_version.to_string(),
  version: ev_view_version,
  sources: sources + marshal_sources + enum_sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_args: lib_ldflags,
  link_depends: lib_symbol_map,
  install: true,
)

libevview_dep = declare_dependency(
  sources: enum_sources[1],
  include_directories: libview_inc,
  dependencies: libevdocument_dep,
  link_with: libevview,
)

pkg.generate(
  libevview,
  filebase: 'evince-view-' + ev_api_version,
  name: 'Evince View',
  description: 'GNOME document viewer view library',
  subdirs: ev_include_subdir,
  requires: [
    'evince-document-@0@ = @1@'.format(ev_api_version, ev_version),
    gthread_dep,
  ],
  variables: 'exec_prefix=${prefix}',
)

# GObject Introspection
if enable_introspection
  incs = [
    'Gdk-3.0',
    'GdkPixbuf-2.0',
    'Gio-2.0',
    'GLib-2.0',
    'GObject-2.0',
    'Gtk-3.0',
    libevdocument_gir[0],
  ]

  gnome.generate_gir(
    [libevview, libevdocument],
    sources: sources + headers + [enum_sources[1]],
    includes: incs,
    nsversion: ev_api_version,
    namespace: 'EvinceView',
    identifier_prefix: ev_code_prefix,
    symbol_prefix: ev_code_prefix.to_lower(),
    export_packages: 'evince-view-' + ev_api_version,
    extra_args: '-DEVINCE_COMPILATION',
    install: true,
  )
endif
