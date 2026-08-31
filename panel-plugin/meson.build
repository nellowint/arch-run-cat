plugin_name = 'runcat'

shared_module(plugin_name,
  'runcat.c',
  dependencies: [gtk, libxfce4panel, libxfce4util, libxfconf, gdk_pixbuf],
  install: true,
  install_dir: get_option('libdir') / 'xfce4' / 'panel' / 'plugins',
  name_prefix: 'lib'
)

install_data('runcat.desktop',
  install_dir: get_option('datadir') / 'xfce4' / 'panel' / 'plugins'
)

# Install cat resources for native plugin
install_data('../resources/cat/dark_cat_0.png',
             '../resources/cat/dark_cat_1.png',
             '../resources/cat/dark_cat_2.png',
             '../resources/cat/dark_cat_3.png',
             '../resources/cat/dark_cat_4.png',
             '../resources/cat/light_cat_0.png',
             '../resources/cat/light_cat_1.png',
             '../resources/cat/light_cat_2.png',
             '../resources/cat/light_cat_3.png',
             '../resources/cat/light_cat_4.png',
             install_dir: get_option('datadir') / 'arch-run-cat' / 'cat'
)
install_data('../resources/cat/dark_cat_0.05.gif',
             '../resources/cat/dark_cat_0.06.gif',
             '../resources/cat/dark_cat_0.07.gif',
             '../resources/cat/dark_cat_0.08.gif',
             '../resources/cat/dark_cat_0.09.gif',
             '../resources/cat/dark_cat_0.10.gif',
             '../resources/cat/light_cat_0.05.gif',
             '../resources/cat/light_cat_0.06.gif',
             '../resources/cat/light_cat_0.07.gif',
             '../resources/cat/light_cat_0.08.gif',
             '../resources/cat/light_cat_0.09.gif',
             '../resources/cat/light_cat_0.10.gif',
             install_dir: get_option('datadir') / 'arch-run-cat' / 'cat'
)
