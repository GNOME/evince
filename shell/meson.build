shell_inc = include_directories('.')

sources = files(
  'ev-annotation-properties-dialog.c',
  'ev-annotations-toolbar.c',
  'ev-application.c',
  'ev-bookmark-action.c',
  'ev-bookmarks.c',
  'ev-file-monitor.c',
  'ev-find-sidebar.c',
  'ev-history.c',
  'ev-keyring.c',
  'ev-loading-message.c',
  'ev-message-area.c',
  'ev-metadata.c',
  'ev-password-view.c',
  'ev-progress-message-area.c',
  'ev-properties-dialog.c',
  'ev-properties-fonts.c',
  'ev-properties-license.c',
  'ev-recent-view.c',
  'ev-toolbar.c',
  'ev-utils.c',
  'ev-window.c',
  'ev-window-title.c',
  'ev-sidebar-annotations.c',
  'ev-sidebar-attachments.c',
  'ev-sidebar-bookmarks.c',
  'ev-sidebar.c',
  'ev-sidebar-layers.c',
  'ev-sidebar-links.c',
  'ev-sidebar-page.c',
  'ev-sidebar-thumbnails.c',
  'ev-zoom-action.c',
  'main.c',
)

evince_css = (gtk_dep.version().version_compare('>= 3.20.0') ? 'evince.css' : 'evince-3.18.css')

resource_data = files(
  '../data/thumbnail-frame.png',
  evince_css,
  'evince-menus.ui',
  'help-overlay.ui',
)

resource_conf = configuration_data()
resource_conf.set('EVINCE_CSS',  evince_css)

resource = 'evince.gresource.xml'

resource_xml = configure_file(
  input: resource + '.in',
  output: resource,
  configuration: resource_conf,
)

sources += gnome.compile_resources(
  'ev-resources',
  resource_xml,
  source_dir: data_dir,
  c_name: ev_code_prefix.to_lower(),
  dependencies: resource_data,
  export: true,
)

shell_deps = [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  libevmisc_dep,
  libevproperties_dep,
  libgd_dep,
  libgimpcellrenderertoggle_dep,
  m_dep,
]

cflags = [
  '-DBINDIR="@0@"'.format(join_paths(ev_prefix, ev_bindir)),
  '-DGNOMEDATADIR="@0@"'.format(join_paths(ev_prefix, ev_datadir)),
  '-DEVINCE_COMPILATION',
  '-DGDK_MULTIHEAD_SAFE',
  '-DGTK_MULTIHEAD_SAFE',
]

ldflags = common_ldflags

if ev_platform == 'win32'
  resource = 'evince-icon.o'

  ldflag += custom_target(
    resource,
    input: 'evince-icon.rc',
    output: resource,
    command: [find_program('windres'), '@INPUT@', '-I', data_dir, '@OUTPUT@'],
  )
else
  shell_deps += adwaita_icon_theme_dep
endif

if enable_dbus
  sources += files('ev-media-player-keys.c')

  gdbus = 'ev-gdbus'

  sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace.to_lower(),
    namespace: ev_code_prefix,
    object_manager: true,
    autocleanup: 'objects',
  )
endif

if enable_keyring
  shell_deps += libsecret_dep
endif

executable(
  'evince',
  sources,
  include_directories: top_inc,
  dependencies: shell_deps,
  c_args: cflags,
  link_args: ldflags,
  install: true,
)

if enable_dbus
  sources = files('ev-daemon.c')

  gdbus = 'ev-daemon-gdbus'

  sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace.to_lower(),
    namespace: ev_code_prefix,
    object_manager: true,
    autocleanup: 'objects',
  )

  deps = [
    gio_dep,
    gio_unix_dep,
  ]

  cflags = [
    '-DBINDIR="@0@"'.format(join_paths(ev_prefix, ev_bindir)),
    '-DEVINCE_COMPILATION',
  ]

  executable(
    'evinced',
    sources,
    include_directories: top_inc,
    dependencies: deps,
    c_args: cflags,
    install: true,
    install_dir: ev_libexecdir,
  )
endif
