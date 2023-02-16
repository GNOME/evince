backend_sources = files(
  'comics-document.c',
  'ev-archive.c',
)

backend_incs = backends_common_incs + [cut_n_paste_inc]

backend_deps = backends_common_deps + [
  libarchive_dep,
]

test_name = 'test-ev-archive'

test_sources = files(
  'ev-archive.c',
  'test-ev-archive.c',
)

executable(
  test_name,
  test_sources,
  include_directories: backend_incs,
  dependencies: backend_deps,
)
