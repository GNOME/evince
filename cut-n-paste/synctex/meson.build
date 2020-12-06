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

# for PathFindExtension
if host_machine.system() == 'windows'
  shlwapi = cc.find_library('shlwapi')
else
  shlwapi = []
endif

synctex_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: shlwapi,
  link_with: libsynctex,
)
