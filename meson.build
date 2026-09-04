project('arch-run-cat', 'c',
  version: '0.4.1',
  license: 'MIT',
  meson_version: '>=0.54.0',
  default_options: ['c_std=gnu11', 'warning_level=1']
)

gtk = dependency('gtk+-3.0', version: '>=3.24')
libxfce4panel = dependency('libxfce4panel-2.0', version: '>=4.18')
libxfce4util = dependency('libxfce4util-1.0')
libxfconf = dependency('libxfconf-0')
gdk_pixbuf = dependency('gdk-pixbuf-2.0')

add_project_arguments('-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')), language: 'c')

subdir('panel-plugin')
