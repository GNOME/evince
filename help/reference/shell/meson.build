doc_module = ev_name

fixxref_args = [
  '--html-dir=' + join_paths(ev_prefix, gnome.gtkdoc_html_dir(doc_module)),
  '--extra-dir=' + libevdocument_doc_path,
  '--extra-dir=' + libevview_doc_path,
] + common_fixxref_args


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
  src_dir: shell_inc,
  ignore_headers: 'ev-marshal.h',
  include_directories: top_inc,
  dependencies: libshell_dep,
  namespace: ev_code_prefix.to_lower(),
  fixxref_args: fixxref_args,
  gobject_typesfile: files(doc_module + '.types'),
  content_files: version,
  install: true,
  install_dir: doc_module,
)
