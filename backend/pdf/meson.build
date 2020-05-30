deps = backends_deps + [
  cairo_pdf_dep,
  cairo_ps_dep,
  libxml_dep,
  poppler_glib_dep,
]

shared_module(
  backend_name,
  sources: 'ev-poppler.cc',
  include_directories: backends_incs,
  dependencies: deps,
  cpp_args: backends_cflags,
  link_args: backends_ldflags,
  link_depends: backends_symbol_map,
  install: true,
  install_dir: ev_backendsdir,
  name_suffix: name_suffix,
)
