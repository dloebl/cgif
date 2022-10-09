project(
  'libcgif',
  'c',
  version : '0.3.0',
  license : 'MIT',
  default_options : ['c_std=c99'],
)

cgif_sources = ['src/cgif.c', 'src/cgif_raw.c']
lib = library(
  'cgif',
  cgif_sources,
  include_directories : ['inc/'],
  soversion : '0',
  version : '0.3.0',
  install : true,
)

install_headers('inc/cgif.h')

import('pkgconfig').generate(
  lib,
  name : 'cgif',
  description : 'A fast and lightweight GIF encoder',
)

if get_option('tests') and not meson.is_cross_build()
  libcgif_dep = declare_dependency(link_with : lib)
  subdir('examples')
  subdir('tests')
endif
