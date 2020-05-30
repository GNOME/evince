subdir('mdvi-lib')

sources = files(
  'cairo-device.c',
  'dvi-document.c',
  'fonts.c',
  'texmfcnf.c',
)

deps = backends_deps + [
  libmdvi_dep,
  libspectre_dep,
  kpathsea_dep,
  t1_dep,
]

shared_module(
  backend_name,
  sources: sources,
  include_directories: backends_incs,
  dependencies: deps,
  c_args: backends_cflags,
  link_args: backends_ldflags,
  link_depends: backends_symbol_map,
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
