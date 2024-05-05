subdir('icons')

# Desktop files
desktop_in = configure_file(
  input: 'org.gnome.Evince.desktop.in.in',
  output: 'org.gnome.Evince.desktop.in',
  configuration: mime_types_conf,
)
i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
  output: '@0@.desktop'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(ev_datadir, 'applications'),
)

i18n.merge_file(
  type: 'desktop',
  input: 'org.gnome.Evince-previewer.desktop.in',
  output: '@0@-previewer.desktop'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(ev_datadir, 'applications'),
)

# DBus service file
if enable_dbus
  service_conf = configuration_data()
  service_conf.set('libexecdir', ev_libexecdir)
  service_conf.set('application_id', application_id)

  configure_file(
      input: 'org.gnome.Evince.Daemon.service.in',
      output: '@0@.Daemon.service'.format(application_id),
      configuration: service_conf,
      install: true,
      install_dir: dbus_service_dir,
    )

  if install_systemd_user_unit_dir
    configure_file(
      input: 'org.gnome.Evince.service.in',
      output: '@0@.service'.format(application_id),
      configuration: service_conf,
      install: true,
      install_dir: systemd_user_unit_dir,
    )  endif
endif

# GSettings schema
gschema_file = 'org.gnome.Evince.gschema.xml'
install_data(
  gschema_file,
  install_dir: join_paths(ev_datadir, 'glib-2.0', 'schemas'),
)
if ev_debug
  gnome.compile_schemas(
    depend_files: gschema_file,
  )
endif

# man file
man_pages = ['evince.1', 'evince-previewer.1', 'evince-thumbnailer.1']
foreach man_page: man_pages
  install_data(
    man_page,
    install_dir: join_paths(ev_mandir, 'man1'),
  )
endforeach

# Metainfo file
metainfo_config = configuration_data()
metainfo_config.set('app_id', application_id)
metainfo_file_in = configure_file(
          input: 'org.gnome.Evince.metainfo.xml.in.in',
         output: 'org.gnome.Evince.metainfo.xml.in',
  configuration: metainfo_config,
)
metainfo_file = i18n.merge_file(
  input: metainfo_file_in,
  output: '@0@.metainfo.xml'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: ev_appstreamdir,
)

# Validate Appdata
appstreamcli = find_program('appstreamcli', required: false)
if (appstreamcli.found())
  test('validate-appdata',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', metainfo_file],
    workdir: meson.current_build_dir()
  )
endif
