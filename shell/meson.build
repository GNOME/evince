evince_sources = files(
  'ev-annotation-properties-dialog.c',
  'ev-annotations-toolbar.c',
  'ev-application.c',
  'ev-bookmarks.c',
  'ev-file-monitor.c',
  'ev-find-sidebar.c',
  'ev-history.c',
  'ev-keyring.c',
  'ev-message-area.c',
  'ev-metadata.c',
  'ev-password-view.c',
  'ev-progress-message-area.c',
  'ev-properties-dialog.c',
  'ev-properties-view.c',
  'ev-properties-fonts.c',
  'ev-properties-license.c',
  'ev-recent-view.c',
  'ev-toolbar.c',
  'ev-thumbnail-item.c',
  'ev-utils.c',
  'ev-window.c',
  'ev-window-title.c',
  'ev-search-box.c',
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

marshal = 'ev-shell-marshal'

marshal_sources = gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: 'ev_shell_marshal',
  internal: true,
  extra_args: '--prototypes',
)

evince_sources += marshal_sources

evince_deps = [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  libaw_dep,
  libevmisc_dep,
  libevdocument_dep,
  libevview_dep,
  libevproperties_dep,
  libdazzle_dep,
  m_dep,
]

evince_cflags = [
  '-DBINDIR="@0@"'.format(ev_bindir),
  '-DEVINCE_COMPILATION',
]

evince_ldflags = common_ldflags

if enable_dbus
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

  evinced_sources += gnome.gdbus_codegen(
    gdbus + '-generated',
    gdbus + '.xml',
    interface_prefix: ev_namespace,
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
