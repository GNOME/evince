sources = files(
  'afmparse.c',
  'bitmap.c',
  'color.c',
  'common.c',
  'dviread.c',
  'files.c',
  'font.c',
  'fontmap.c',
  'fontsrch.c',
  'gf.c',
  'hash.c',
  'list.c',
  'pagesel.c',
  'paper.c',
  'pk.c',
  'setup.c',
  'special.c',
  'sp-epsf.c',
  'tfm.c',
  'tfmfile.c',
  'tt.c',
  'util.c',
  'vf.c',
)

libmdvi = static_library(
  'mdvi',
  sources: sources,
  include_directories: top_inc,
  gnu_symbol_visibility: 'hidden',
)

libmdvi_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libmdvi,
)
