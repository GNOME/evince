expand_content_md_files = [
  'profiling.md',
]

libevview_toml = configure_file(
  input: 'libevview.toml.in',
  output: 'libevview.toml',
  configuration: toml_conf,
  install: true,
  install_dir: docs_dir / 'libevview',
)

custom_target('libevview-doc',
  input: [ libevview_toml, libevview_gir[0] ],
  output: 'libevview',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--config=@INPUT0@',
    '--add-include-path=@0@'.format(meson.current_build_dir() / '..' / '..' / '..' / 'libdocument'),
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depends: [ libevdocument_gir[0] ],
  build_by_default: true,
  depend_files: [ expand_content_md_files ],
  install: true,
  install_dir: docs_dir,
)
