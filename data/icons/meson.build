# array: [size, actions, mimetypes, apps]
icons = []

actions = [
  'object-rotate-left.png',
  'object-rotate-right.png',
  'view-page-continuous.png',
  'view-page-facing.png',
  'zoom-fit-height.png',
  'zoom-fit-width.png',
  'zoom.png',
]

icons += [['16x16', actions, ['x-office-presentation.png'], [ev_namespace + '.png']]]

actions = [
  'eye.png',
  'object-rotate-left.png',
  'object-rotate-right.png',
  'view-page-continuous.png',
  'view-page-facing.png',
  'zoom-fit-page.png',
  'zoom-fit-width.png',
  'zoom.png',
]

icons += [['22x22', actions, ['x-office-presentation.png'], [ev_namespace + '.png']]]

actions = [
  'object-rotate-left.png',
  'object-rotate-right.png',
  'stock_filters-invert.png',
  'view-page-continuous.png',
  'view-page-facing.png',
  'zoom-fit-height.png',
  'zoom-fit-width.png',
  'zoom.png',
]

icons += [['24x24', actions, ['x-office-presentation.png'], [ev_namespace + '.png']]]

actions = [
  'object-rotate-left.png',
  'object-rotate-right.png',
  'view-page-continuous.png',
  'view-page-facing.png',
]

icons += [['32x32', actions, ['x-office-presentation.png'], []]]

actions = [
  'close.png',
  'resize-se.png',
  'resize-sw.png',
  'view-page-continuous.png',
  'view-page-facing.png',
]

icons += [['48x48', actions, [], [ev_namespace + '.png']]]

icons += [['256x256', [], [], [ev_namespace + '.png']]]

actions = [
  'object-rotate-left.svg',
  'object-rotate-right.svg',
  'outline-symbolic.svg',
  'view-sidebar-symbolic.svg',
]

icons += [['scalable', actions, ['x-office-presentation.svg'], []]]

icons += [['symbolic', [], [], [ev_namespace + '-symbolic.svg']]]

foreach icon: icons
  foreach action: icon[1]
    install_data(
      join_paths(icon[0], 'actions', action),
      install_dir: join_paths(ev_pkgdatadir, 'icons', 'hicolor', icon[0], 'actions'),
    )
  endforeach

  foreach mimetype: icon[2]
    install_data(
      join_paths(icon[0], 'mimetypes', mimetype),
      install_dir: join_paths(ev_pkgdatadir, 'icons', 'hicolor', icon[0], 'mimetypes'),
    )
  endforeach

  foreach app: icon[3]
    install_data(
      join_paths(icon[0], 'apps', app),
      install_dir: join_paths(ev_datadir, 'icons', 'hicolor', icon[0], 'apps'),
    )
  endforeach
endforeach
