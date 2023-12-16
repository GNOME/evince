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
  output: '@0@.desktop'.format(ev_app_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(ev_datadir, 'applications'),
)

i18n.merge_file(
  type: 'desktop',
  input: 'org.gnome.Evince-previewer.desktop.in',
  output: '@0@-previewer.desktop'.format(ev_app_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(ev_datadir, 'applications'),
)

# DBus service file
if enable_dbus
  service_conf = configuration_data()
  service_conf.set('libexecdir', ev_libexecdir)

  # The D-Bus interface names are in lowercase, which does not match
  # the application name nor the namespace. However, we keep it in
  # lowercase for backward compatibility.
  services = [[ev_namespace.to_lower() + '.Daemon.service', dbus_service_dir]]

  if install_systemd_user_unit_dir
    services += [[ev_namespace + '.service', systemd_user_unit_dir]]
  endif

  foreach service: services
    configure_file(
      input: service[0] + '.in',
      output: service[0],
      configuration: service_conf,
      install: true,
      install_dir: service[1],
    )
  endforeach
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
metainfo_config.set('app_id', ev_app_id)
metainfo_file_in = configure_file(
          input: 'org.gnome.Evince.metainfo.xml.in.in',
         output: 'org.gnome.Evince.metainfo.xml.in',
  configuration: metainfo_config,
)
metainfo_file = i18n.merge_file(
  input: metainfo_file_in,
  output: '@0@.metainfo.xml'.format(ev_app_id),
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
