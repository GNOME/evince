backend_sources = files(
  'djvu-document.c',
  'djvu-links.c',
  'djvu-text-page.c',
)

backend_deps = backends_common_deps + [ddjvuapi_dep]
backend_cflags = backends_common_cflags + ['-DGNOMEICONDIR="@0@"'.format(join_paths(ev_datadir, 'pixmaps'))]
