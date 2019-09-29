sources = files(
  'synctex_parser.c',
  'synctex_parser_utils.c',
)

deps = [zlib_dep]

if ev_platform == 'win32'
  deps += shlwapi_dep
endif

libsynctex = static_library(
  'synctex',
  sources: sources,
  dependencies: deps,
  include_directories: top_inc,
  c_args: '-DSYNCTEX_VERBOSE=0',
)

synctex_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libsynctex,
)
