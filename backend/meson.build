backends_incs = [top_inc]

backends_deps = [libevdocument_dep]

backends_cflags = [
  '-DGNOMELOCALEDIR="@0@"'.format(ev_localedir),
  '-DEVINCE_COMPILATION',
  '-DGDK_MULTIHEAD_SAFE',
  '-DGTK_MULTIHEAD_SAFE',
]

backends_symbol_map = join_paths(meson.current_source_dir(), 'backend-symbol.map')
backends_ldflags = cpp.get_supported_link_arguments('-Wl,--version-script,' + backends_symbol_map)

foreach backend, backend_mime_types: backends
  backend_name = backend + 'document'

  backend_mime_types_conf = configuration_data()
  backend_mime_types_conf.set(backend.to_upper() + '_MIME_TYPES', ';'.join(backend_mime_types))

  appdata_mime_types = ''
  foreach type: backend_mime_types
    appdata_mime_types += '\t<mimetype>@0@</mimetype>\n'.format(type)
  endforeach

  backend_mime_types_conf.set('APPDATA_@0@_MIME_TYPES'.format(backend.to_upper()), appdata_mime_types)

  backend_desc = backend_name + '.evince-backend'

  backend_desc_in = configure_file(
    input: '@0@/@1@.in.in'.format(backend, backend_desc),
    output: backend_desc + '.in',
    configuration: backend_mime_types_conf,
  )

  custom_target(
    backend_desc,
    input: backend_desc_in,
    output: backend_desc,
    command: intltool_desktop_cmd,
    install: true,
    install_dir: ev_backendsdir,
  )

  appstream = 'evince-@0@.metainfo.xml'.format(backend_name)

  appstream_in = configure_file(
    input: '@0@/@1@.in.in'.format(backend, appstream),
    output: appstream + '.in',
    configuration: backend_mime_types_conf,
  )

  custom_target(
    appstream,
    input: appstream_in,
    output: appstream,
    command: intltool_xml_cmd,
    install: true,
    install_dir: ev_appstreamdir,
  )

  subdir(backend)
endforeach
