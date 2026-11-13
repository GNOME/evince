test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('EV_BACKENDS_DIR', meson.project_build_root() / 'backend')
test_env.set('G_DEBUG', 'gc-friendly')

if enable_pdf
  test_ev_document = executable(
    'test-ev-document',
    'test-ev-document.c',
    include_directories: top_inc,
    dependencies: libevdocument_dep,
  )

  test(
    'test-ev-document',
    test_ev_document,
    env: test_env,
    protocol: 'tap',
    args: ['--tap'],
    suite: 'libdocument',
  )
endif
