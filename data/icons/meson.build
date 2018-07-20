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

icons += [['16x16', actions, ['x-office-presentation.png']]]

actions = [
  'object-rotate-left.png',
  'object-rotate-right.png',
  'view-page-continuous.png',
  'view-page-facing.png',
  'zoom-fit-page.png',
  'zoom-fit-width.png',
  'zoom.png',
]

icons += [['22x22', actions, ['x-office-presentation.png']]]

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

icons += [['24x24', actions, ['x-office-presentation.png']]]

actions = [
  'object-rotate-left.png',
  'object-rotate-right.png',
  'view-page-continuous.png',
  'view-page-facing.png',
]

icons += [['32x32', actions, ['x-office-presentation.png']]]

actions = [
  'close.png',
  'resize-se.png',
  'resize-sw.png',
  'view-page-continuous.png',
  'view-page-facing.png',
]

icons += [['48x48', actions, []]]

icons += [['256x256', [], []]]

actions = [
  'annotations-text-symbolic.svg',
  'annotations-squiggly-symbolic.svg',
  'find-unsupported-symbolic.svg',
  'object-rotate-left.svg',
  'object-rotate-right.svg',
  'outline-symbolic.svg',
  'view-sidebar-symbolic.svg',
  'visible-symbolic.svg',
]

icons += [['scalable', actions, ['x-office-presentation.svg']]]

icons += [['symbolic', [], []]]

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
endforeach

install_data(
  join_paths('scalable', 'apps', '@0@.svg'.format(ev_namespace)),
  install_dir: join_paths(ev_datadir, 'icons', 'hicolor', 'scalable', 'apps'),
)

install_data(
  join_paths('symbolic', 'apps', '@0@-symbolic.svg'.format(ev_namespace)),
  install_dir: join_paths(ev_datadir, 'icons', 'hicolor', 'symbolic', 'apps'),
)
