sources = files(
  'common/conv.c',
  'common/crc32.c',
  'common/stream.c',
  'common/unarr.c',
  'lzmasdk/CpuArch.c',
  'lzmasdk/Ppmd7.c',
  'lzmasdk/Ppmd7Dec.c',
  'rar/filter-rar.c',
  'rar/huffman-rar.c',
  'rar/parse-rar.c',
  'rar/rar.c',
  'rar/rarvm.c',
  'rar/uncompress-rar.c',
)

libunarr = static_library(
  'unarr',
  sources: sources,
  dependencies: zlib_dep,
  c_args: [ '-DHAVE_ZLIB', '-DNDEBUG' ]
)

libunarr_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libunarr,
)
