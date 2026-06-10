project(
  'libcgif',
  'c',
  version : '0.6.0',
  meson_version: '>=0.56',
  license : 'MIT',
  default_options : ['c_std=c99'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

cgif_sources = ['src/cgif.c', 'src/cgif_raw.c']
cgif_headers = ['inc/cgif.h']

# EXPERIMENTAL: the RGB API is opt-in and disabled by default.
# When enabled it is compiled into the public library and its header installed.
if get_option('experimental_rgb')
  cgif_sources += 'src/cgif_rgb.c'
  cgif_headers += 'inc/cgif_rgb.h'
endif

lib = library(
  'cgif',
  cgif_sources,
  dependencies : m_dep,
  include_directories : ['inc/'],
  soversion : '0',
  version : meson.project_version(),
  install : true,
)

install_headers(cgif_headers)

import('pkgconfig').generate(
  lib,
  name : 'cgif',
  description : 'A fast and lightweight GIF encoder',
)

libcgif_dep = declare_dependency(link_with : lib)

if get_option('tests') and not meson.is_cross_build()
  # Internal (non-installed) library that always includes the experimental RGB
  # API so the RGB tests keep building even when 'experimental_rgb' is off.
  cgif_rgb_test_lib = static_library(
    'cgif_rgb_test',
    ['src/cgif.c', 'src/cgif_raw.c', 'src/cgif_rgb.c'],
    dependencies : m_dep,
    include_directories : ['inc/'],
    install : false,
  )
  libcgif_rgb_dep = declare_dependency(
    link_with : cgif_rgb_test_lib,
    include_directories : ['inc/'],
  )
  subdir('tests')
  if get_option('fuzzer')
    subdir('fuzz')
  endif
endif

if get_option('examples')
  subdir('examples')
endif
