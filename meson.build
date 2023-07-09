project(
  'libcgif',
  'c',
  version : '0.4.0',
  license : 'MIT',
  default_options : ['c_std=c99'],
)

cgif_sources = ['src/cgif.c', 'src/cgif_raw.c']
lib = library(
  'cgif',
  cgif_sources,
  include_directories : ['inc/'],
  soversion : '0',
  version : meson.project_version(),
  install : true,
)

install_headers('inc/cgif.h')

import('pkgconfig').generate(
  lib,
  name : 'cgif',
  description : 'A fast and lightweight GIF encoder',
)

libcgif_dep = declare_dependency(link_with : lib)

if get_option('tests') and not meson.is_cross_build()
  subdir('tests')
  if get_option('fuzzer')
    subdir('fuzz')
  endif
endif

if get_option('examples')
  subdir('examples')
endif
