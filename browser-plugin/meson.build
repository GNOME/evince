sources = files(
  'EvBrowserPlugin.cpp',
  'EvBrowserPluginMain.cpp',
  'EvBrowserPluginToolbar.cpp',
)

resource_data = files(
  '../data/thumbnail-frame.png',
  'evince-browser.css',
)

resource = 'EvBrowserPlugin'

sources += gnome.compile_resources(
  resource + 'Resources',
  resource + '.gresource.xml',
  source_dir: data_dir,
  dependencies: resource_data,
  export: true,
)

incs = [
  top_inc,
  include_directories('npapi'),
]

cflags = [
  '-DGNOMELOCALEDIR="@0@"'.format(ev_localedir),
  '-DEVINCE_COMPILATION',
  '-DMOZ_X11',
  '-DXP_UNIX',
]

cppflags = cflags + cc.get_supported_arguments([
  '-std=c++11',
  '-fno-exceptions',
  '-fno-strict-aliasing',
  '-fno-rtti',
])

symbol_map = join_paths(meson.current_source_dir(), 'plugin-symbol.map')
ldflag = cpp.get_supported_link_arguments([
  '-Wl,--version-script,' + symbol_map,
  '-Wl,-z,nodelete',
])

shared_module(
  'evbrowserplugin',
  sources: sources,
  include_directories: incs,
  dependencies: libevmisc_dep,
  cpp_args: cppflags,
  install: true,
  install_dir: browser_plugin_dir,
  name_suffix: name_suffix,
)
