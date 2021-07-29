tests = [
  'all_optim',
  'animated_stripe_pattern',
  'global_plus_local_table',
  'max_color_table_test',
  'min_color_table_test',
  'only_local_table',
  'overlap_everything',
  'overlap_some_rows',
  'write_fn',
]

foreach t : tests
  test_exe = executable(
    'test_' + t,
    t + '.c',
    dependencies : [libcgif_dep],
    include_directories : ['..'],
  )
  test(t, test_exe)
endforeach
