shared_module(
  backend_name,
  sources: 'xps-document.c',
  include_directories: backends_incs,
  dependencies: backends_deps + [libgxps_dep],
  c_args: backends_cflags,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
