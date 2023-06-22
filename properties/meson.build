libevproperties = static_library(
  'evproperties',
  sources: ['ev-properties.c'],
  include_directories: top_inc,
  dependencies: libevdocument_dep,
  gnu_symbol_visibility: 'hidden',
)

libevproperties_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: libevdocument_dep,
  link_with: libevproperties,
)

if enable_nautilus
  deps = [
    libevdocument_dep,
    libnautilus_extension_dep,
  ]

  shared_module(
    'evince-properties-page',
    sources: 'ev-properties-main.c',
    include_directories: top_inc,
    dependencies: deps,
    link_with: libevproperties,
    install: true,
    install_dir: nautilus_extension_dir,
    gnu_symbol_visibility: 'hidden',
  )
endif
