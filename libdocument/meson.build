libdocument_inc = include_directories('.')

include_subdir = join_paths(ev_include_subdir, 'libdocument')

headers = files(
  'ev-annotation.h',
  'ev-async-renderer.h',
  'ev-attachment.h',
  'ev-backends-manager.h',
  'ev-document-annotations.h',
  'ev-document-attachments.h',
  'ev-document-factory.h',
  'ev-document-find.h',
  'ev-document-fonts.h',
  'ev-document-forms.h',
  'ev-document-images.h',
  'ev-document-info.h',
  'ev-document-layers.h',
  'ev-document-links.h',
  'ev-document-media.h',
  'ev-document-misc.h',
  'ev-document-print.h',
  'ev-document-security.h',
  'ev-document-text.h',
  'ev-document-transition.h',
  'ev-document.h',
  'ev-file-exporter.h',
  'ev-file-helpers.h',
  'ev-form-field.h',
  'ev-image.h',
  'ev-init.h',
  'ev-layer.h',
  'ev-link-action.h',
  'ev-link-dest.h',
  'ev-link.h',
  'ev-macros.h',
  'ev-mapping-list.h',
  'ev-media.h',
  'ev-page.h',
  'ev-render-context.h',
  'ev-selection.h',
  'ev-transition-effect.h',
)

install_headers(
  headers,
  subdir: include_subdir,
)

sources = files(
  'ev-annotation.c',
  'ev-async-renderer.c',
  'ev-attachment.c',
  'ev-backend-info.c',
  'ev-debug.c',
  'ev-document.c',
  'ev-document-annotations.c',
  'ev-document-attachments.c',
  'ev-document-factory.c',
  'ev-document-find.c',
  'ev-document-fonts.c',
  'ev-document-forms.c',
  'ev-document-images.c',
  'ev-document-info.c',
  'ev-document-layers.c',
  'ev-document-links.c',
  'ev-document-media.c',
  'ev-document-misc.c',
  'ev-document-print.c',
  'ev-document-security.c',
  'ev-document-text.c',
  'ev-document-transition.c',
  'ev-file-exporter.c',
  'ev-file-helpers.c',
  'ev-form-field.c',
  'ev-image.c',
  'ev-init.c',
  'ev-layer.c',
  'ev-link-action.c',
  'ev-link.c',
  'ev-link-dest.c',
  'ev-mapping-list.c',
  'ev-media.c',
  'ev-module.c',
  'ev-page.c',
  'ev-render-context.c',
  'ev-selection.c',
  'ev-transition-effect.c',
  'ev-xmp.c',
  'ev-xmp.h',
)

version_conf = configuration_data()
version_conf.set('EV_MAJOR_VERSION', ev_major_version)
version_conf.set('EV_MINOR_VERSION', ev_minor_version)

version = 'ev-version.h'

version_header = configure_file(
  input: version + '.in',
  output: version,
  configuration: version_conf,
  install: true,
  install_dir: join_paths(ev_includedir, include_subdir),
)

enum_types = 'ev-document-type-builtins'

enum_sources = gnome.mkenums(
  enum_types,
  sources: headers,
  h_template: enum_types + '.h.template',
  c_template: enum_types + '.c.template',
  install_header: true,
  install_dir: join_paths(ev_includedir, include_subdir),
)

common_deps = [
  cairo_dep,
  gdk_pixbuf_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
]

deps = common_deps + [
  gmodule_dep,
  gmodule_no_export_dep,
  libxml_dep,
  m_dep,
  synctex_dep,
  zlib_dep,
]

cflags = [
  '-DG_LOG_DOMAIN="EvinceDocument"',
  '-DGNOMELOCALEDIR="@0@"'.format(ev_localedir),
  '-DEV_BACKENDSDIR="@0@"'.format(ev_backendsdir),
  '-DEV_BACKENDSBINARYVERSION="@0@"'.format(ev_backends_binary_version),
  '-DEVINCE_COMPILATION',
]

libevdocument = shared_library(
  'evdocument3',
  version: ev_document_version,
  sources: sources + enum_sources + [version_header],
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  gnu_symbol_visibility: 'hidden',
)

libevdocument_dep = declare_dependency(
  sources: headers + [enum_sources[1]] + [version_header],
  include_directories: libdocument_inc,
  dependencies: deps,
  link_with: libevdocument,
)

pkg.generate(
  libevdocument,
  filebase: 'evince-document-' + ev_api_version,
  name: 'Evince Document',
  description: 'GNOME document viewer backend library',
  subdirs: ev_include_subdir,
  requires: [
    gio_dep,
    gtk_dep,
  ],
  variables: [
    'exec_prefix=${prefix}',
    'backenddir=' + join_paths('${libdir}', ev_backends_subdir),
  ],
)

# GObject Introspection
if enable_introspection
  incs = [
    'Gdk-3.0',
    'GdkPixbuf-2.0',
    'Gio-2.0',
    'GLib-2.0',
    'Gtk-3.0',
  ]

  libevdocument_gir = gnome.generate_gir(
    libevdocument,
    sources: sources + headers + [enum_sources[1], version_header],
    includes: incs,
    nsversion: ev_api_version,
    namespace: 'EvinceDocument',
    identifier_prefix: ev_code_prefix,
    symbol_prefix: ev_code_prefix.to_lower(),
    export_packages: 'evince-document-' + ev_api_version,
    extra_args: '-DEVINCE_COMPILATION',
    install: true,
  )
endif
