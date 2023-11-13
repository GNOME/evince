libevdocument_toml = configure_file(
  input: 'libevdocument.toml.in',
  output: 'libevdocument-' + ev_api_version + '.toml',
  configuration: toml_conf,
  install: true,
  install_dir: docs_dir / 'libevdocument',
)

custom_target('libevdocument-doc',
  input: [ libevdocument_toml, libevdocument_gir[0] ],
  output: 'libevdocument',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: docs_dir,
)
