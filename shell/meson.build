shell_inc = include_directories('.')

sources = files(
  'ev-annotation-properties-dialog.c',
  'ev-annotations-toolbar.c',
  'ev-application.c',
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
)

resource_data = files(
  'evince.css',
  '../data/thumbnail-frame.png',
  'evince-menus.ui',
  'help-overlay.ui',
)

sources += gnome.compile_resources(
  'ev-resources',
  'evince.gresource.xml',
  source_dir: data_dir,
  c_name: ev_code_prefix.to_lower(),
  dependencies: resource_data,
  export: true,
)

deps = [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  hdy_dep,
  libevmisc_dep,
  libevproperties_dep,
  libdazzle_dep,
  libgd_dep,
  libgimpcellrenderertoggle_dep,
  m_dep,
]

cflags = [
  '-DBINDIR="@0@"'.format(ev_bindir),
  '-DGNOMEDATADIR="@0@"'.format(ev_datadir),
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
  deps += adwaita_icon_theme_dep
endif

if enable_dbus
  sources += files('ev-media-player-keys.c')

  gdbus = 'ev-gdbus'

  sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace,
    namespace: ev_code_prefix,
    object_manager: true,
    autocleanup: 'objects',
  )
endif

if enable_keyring
  deps += libsecret_dep
endif

libshell = shared_library(
  'shell' + ev_major_version.to_string(),
  version: ev_view_version,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
)

libshell_dep = declare_dependency(
  include_directories: shell_inc,
  link_with: libshell,
)

executable(
  'evince',
  'main.c',
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  objects: libshell.extract_all_objects(),
  link_args: ldflags,
  install: true,
)

if enable_dbus
  sources = files('ev-daemon.c')

  gdbus = 'ev-daemon-gdbus'

  # The D-Bus interface names are in lowercase, which does not match
  # the application name nor the namespace. However, we keep the
  # interface prefix in lowercase for backward compatibility.
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
    '-DBINDIR="@0@"'.format(ev_bindir),
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
