input = 'evince.thumbnailer'

configure_file(
  input: input + '.in',
  output: input,
  configuration: mime_types_conf,
  install: true,
  install_dir: join_paths(ev_datadir, 'thumbnailers'),
)

program = 'evince-thumbnailer'

executable(
  program,
  program + '.c',
  include_directories: top_inc,
  dependencies: libevdocument_dep,
  link_args: common_ldflags,
  install: true,
)
