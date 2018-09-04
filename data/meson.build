subdir('icons')

# Desktop file
desktops = [ev_namespace + '.desktop']

if enable_previewer
  desktops += [ev_namespace + '-previewer.desktop']
endif

foreach desktop: desktops
  desktop_in = configure_file(
    input: desktop + '.in.in',
    output: desktop + '.in',
    configuration: mime_types_conf,
  )

  custom_target(
    desktop,
    input: desktop_in,
    output: desktop,
    command: intltool_desktop_cmd,
    install: true,
    install_dir: join_paths(ev_datadir, 'applications'),
  )
endforeach

# DBus service file
if enable_dbus
  service_conf = configuration_data()
  service_conf.set('libexecdir', join_paths(ev_prefix, ev_libexecdir))
  service_conf.set('if_systemd_service', '')

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
gschema = ev_namespace + '.gschema.xml'

custom_target(
  gschema,
  input: gschema + '.in',
  output: gschema,
  command: [intltool_merge, '-x', '-u', '--no-translations', '@INPUT@', '@OUTPUT@'],
  install: true,
  install_dir: join_paths(ev_datadir, 'glib-2.0', 'schemas'),
)

install_data(
  'evince.convert',
  install_dir: join_paths(ev_datadir, 'GConf', 'gsettings'),
)

# man file
install_data(
  'evince.1',
  install_dir: join_paths(ev_mandir, 'man1'),
)

# UI descriptions
install_data(
  'hand-open.png',
  install_dir: ev_pkgdatadir,
)
