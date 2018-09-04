sources = files(
  'synctex_parser.c',
  'synctex_parser_utils.c',
)

libsynctex = static_library(
  'synctex',
  sources: sources,
  include_directories: top_inc,
  c_args: '-DSYNCTEX_VERBOSE=0',
)

synctex_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libsynctex,
)
