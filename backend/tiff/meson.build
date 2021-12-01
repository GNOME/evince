sources = files(
  'tiff-document.c',
  'tiff2ps.c',
)

shared_module(
  backend_name,
  sources: sources,
  include_directories: backends_incs,
  dependencies: backends_deps + [libtiff_dep, m_dep],
  c_args: backends_cflags,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
