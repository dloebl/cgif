project(
  'libcgif',
  'c',
  version : '0.5.0',
  meson_version: '>=0.56',
  license : 'MIT',
  default_options : ['c_std=c99'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

cgif_sources = ['src/cgif.c', 'src/cgif_raw.c', 'src/cgif_rgb.c']
lib = library(
  'cgif',
  cgif_sources,
  dependencies : m_dep,
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
