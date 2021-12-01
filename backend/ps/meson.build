shared_module(
  backend_name,
  sources: 'ev-spectre.c',
  include_directories: backends_incs,
  dependencies: backends_deps + [libspectre_dep],
  c_args: backends_cflags,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
