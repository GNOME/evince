shared_module(
  backend_name,
  sources: 'ev-spectre.c',
  include_directories: backends_incs,
  dependencies: backends_deps + [libspectre_dep],
  c_args: backends_cflags,
  link_args: backends_ldflags,
  link_depends: backends_symbol_map,
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
