input = 'evince.thumbnailer'

configure_file(
  input: input + '.in',
  output: input,
  configuration: mime_types_conf,
  install: true,
  install_dir: join_paths(ev_datadir, 'thumbnailers'),
)

thumbnailer_sources = files(
  'evince-thumbnailer.c',
)

thumbnailer_deps = [
  libevdocument_dep,
]

thumbnailer = executable(
  'evince-thumbnailer',
  sources: thumbnailer_sources,
  include_directories: top_inc,
  dependencies: thumbnailer_deps,
  link_args: common_ldflags,
  install: true,
)
