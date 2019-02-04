sources = files(
  'synctex_parser.c',
  'synctex_parser_utils.c',
)

synctex_deps = [zlib_dep]

if build_machine.system() == 'windows'
  synctex_deps += cc.find_library('shlwapi')
endif

libsynctex = static_library(
  'synctex',
  sources: sources,
  dependencies: synctex_deps,
  include_directories: top_inc,
  c_args: '-DSYNCTEX_VERBOSE=0',
)

synctex_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libsynctex,
)
