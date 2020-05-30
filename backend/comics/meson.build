sources = files(
  'comics-document.c',
  'ev-archive.c',
)

incs = backends_incs + [cut_n_paste_inc]

deps = backends_deps + [
  libarchive_dep,
  libunarr_dep,
]

shared_module(
  backend_name,
  sources: sources,
  include_directories: incs,
  dependencies: deps,
  c_args: backends_cflags,
  link_args: backends_ldflags,
  link_depends: backends_symbol_map,
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)

test_name = 'test-ev-archive'

sources = files(
  'ev-archive.c',
  'test-ev-archive.c',
)

executable(
  test_name,
  sources,
  include_directories: incs,
  dependencies: deps,
)
