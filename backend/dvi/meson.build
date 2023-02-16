subdir('mdvi-lib')

backend_sources = files(
  'cairo-device.c',
  'dvi-document.c',
  'fonts.c',
  'texmfcnf.c',
)

backend_deps = backends_common_deps + [
  libmdvi_dep,
  libspectre_dep,
  kpathsea_dep,
  m_dep
]
