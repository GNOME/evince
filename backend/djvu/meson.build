sources = files(
  'djvu-document.c',
  'djvu-links.c',
  'djvu-text-page.c',
)

shared_module(
  backend_name,
  sources: sources,
  include_directories: backends_incs,
  dependencies: backends_deps + [ddjvuapi_dep],
  c_args: backends_cflags + ['-DGNOMEICONDIR="@0@"'.format(join_paths(ev_datadir, 'pixmaps'))],
  link_args: backends_ldflags,
  link_depends: backends_symbol_map,
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
