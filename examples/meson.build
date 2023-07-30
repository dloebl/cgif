examples = [
  'cgif_example',
  'cgif_example_video',
]

b_install_examples = get_option('install_examples')

foreach e : examples
  ex_exe = executable(
    e,
    e + '.c',
    dependencies : [libcgif_dep],
    include_directories : ['../inc/'],
    install : b_install_examples,
  )
  if get_option('tests') and not meson.is_cross_build()
    test(e, ex_exe)
  endif
endforeach
