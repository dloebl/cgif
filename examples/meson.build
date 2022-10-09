examples = [
  'cgif_example',
  'cgif_example_video',
]

foreach e : examples
  ex_exe = executable(
    'ex_' + e,
    e + '.c',
    dependencies : [libcgif_dep],
    include_directories : ['../inc/'],
  )
  test(e, ex_exe)
endforeach
