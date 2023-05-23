evince_sources = files(
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
  'main.c',
)

evince_sources += gnome.compile_resources(
  'ev-resources',
  'evince.gresource.xml',
  source_dir: data_dir,
  c_name: ev_code_prefix.to_lower(),
)

evince_deps = [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  hdy_dep,
  libevmisc_dep,
  libevdocument_dep,
  libevview_dep,
  libevproperties_dep,
  libdazzle_dep,
  libgd_dep,
  libgimpcellrenderertoggle_dep,
  m_dep,
]

evince_cflags = [
  '-DBINDIR="@0@"'.format(ev_bindir),
  '-DEVINCE_COMPILATION',
]

evince_ldflags = common_ldflags

if enable_dbus
  evince_sources += files('ev-media-player-keys.c')

  gdbus = 'ev-gdbus'

  evince_sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace,
    namespace: ev_code_prefix,
    object_manager: true,
    autocleanup: 'objects',
  )
endif

if enable_keyring
  evince_deps += libsecret_dep
endif

evince = executable(
  'evince',
  sources: evince_sources,
  include_directories: top_inc,
  dependencies: evince_deps,
  c_args: evince_cflags,
  install: true,
)

if enable_dbus
  evinced_sources = files('ev-daemon.c')

  gdbus = 'ev-daemon-gdbus'

  # The D-Bus interface names are in lowercase, which does not match
  # the application name nor the namespace. However, we keep the
  # interface prefix in lowercase for backward compatibility.
  evinced_sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace.to_lower(),
    namespace: ev_code_prefix,
    object_manager: true,
    autocleanup: 'objects',
  )

  evinced_deps = [
    gio_dep,
    gio_unix_dep,
  ]

  evinced_cflags = [
    '-DBINDIR="@0@"'.format(ev_bindir),
    '-DEVINCE_COMPILATION',
  ]

  evinced = executable(
    'evinced',
    evinced_sources,
    include_directories: top_inc,
    dependencies: evinced_deps,
    c_args: evinced_cflags,
    install: true,
    install_dir: ev_libexecdir,
  )
endif
