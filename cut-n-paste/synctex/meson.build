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
