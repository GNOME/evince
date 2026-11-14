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

test_mdvi_fonts = executable(
  'test-mdvi-fonts',
  'test-mdvi-fonts.c',
  include_directories: top_inc,
  dependencies: [glib_dep, libmdvi_dep, kpathsea_dep, m_dep],
)

test(
  'test-mdvi-fonts',
  test_mdvi_fonts,
  protocol: 'tap',
  args: ['--tap'],
  suite: ['backends', 'dvi'],
)

test_mdvi_pk = executable(
  'test-mdvi-pk',
  'test-mdvi-pk.c',
  include_directories: top_inc,
  dependencies: [glib_dep, libmdvi_dep, kpathsea_dep, m_dep],
)

test(
  'test-mdvi-pk',
  test_mdvi_pk,
  protocol: 'tap',
  args: ['--tap'],
  suite: ['backends', 'dvi'],
)
