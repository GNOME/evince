doc_module = 'libevview'
doc_module_dir = '@0@-@1@'.format(doc_module, ev_api_version)

private_headers = [
  'ev-link-accessible.h',
  'ev-pixbuf-cache.h',
  'ev-timeline.h',
  'ev-transition-animation.h',
  'ev-view-accessible.h',
  'ev-view-marshal.h',
  'ev-view-private.h',
]

fixxref_args = [
  '--html-dir=' + join_paths(ev_prefix, gnome.gtkdoc_html_dir(doc_module_dir)),
  '--extra-dir=' + join_paths(glib_prefix, gnome.gtkdoc_html_dir('glib')),
  '--extra-dir=' + join_paths(glib_prefix, gnome.gtkdoc_html_dir('gio')),
  '--extra-dir=' + join_paths(gtk_prefix, gnome.gtkdoc_html_dir('gdk')),
  '--extra-dir=' + join_paths(gtk_prefix, gnome.gtkdoc_html_dir('gdk-pixbuf')),
  '--extra-dir=' + join_paths(gtk_prefix, gnome.gtkdoc_html_dir('gtk')),
]

version_conf = configuration_data()
version_conf.set('VERSION', ev_version)

version_xml = 'version.xml'

version = configure_file(
  input: version_xml + '.in',
  output: version_xml,
  configuration: version_conf,
)

gnome.gtkdoc(
  doc_module,
  main_xml: doc_module + '-docs.xml',
  src_dir: libview_inc,
  ignore_headers: private_headers,
  include_directories: top_inc,
  dependencies: libevview_dep,
  c_args: '-DEVINCE_COMPILATION',
  namespace: ev_code_prefix.to_lower(),
  fixxref_args: fixxref_args,
  gobject_typesfile: doc_module + '.types',
  content_files: version,
  install: true,
  install_dir: doc_module_dir,
)
