test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('EV_BACKENDS_DIR', meson.project_build_root() / 'backend')
test_env.set('G_DEBUG', 'gc-friendly')

test_backend_cflags = []
foreach backend: ['comics', 'djvu', 'dvi', 'pdf', 'ps', 'tiff', 'xps']
  test_backend_cflags += '-DEV_TEST_HAVE_@0@=@1@'.format(
    backend.to_upper(), backends.has_key(backend) ? 'TRUE' : 'FALSE')
endforeach

libevtest = static_library(
  'evtest',
  'ev-test-utils.c',
  include_directories: top_inc,
  dependencies: libevdocument_dep,
)

libevtest_dep = declare_dependency(
  link_with: libevtest,
  dependencies: libevdocument_dep,
)

libdocument_tests = {
  'test-ev-backends': test_backend_cflags,
}

if enable_pdf
  libdocument_tests += {'test-ev-document': []}
endif

foreach name, cflags: libdocument_tests
  exe = executable(
    name,
    name + '.c',
    include_directories: top_inc,
    dependencies: libevtest_dep,
    c_args: cflags,
  )

  test(
    name,
    exe,
    env: test_env,
    protocol: 'tap',
    args: ['--tap'],
    suite: 'libdocument',
  )
endforeach
