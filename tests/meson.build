tests = [
  'all_optim',
  'animated_color_gradient',
  'animated_single_pixel',
  'animated_snake',
  'animated_stripes_horizontal',
  'animated_stripe_pattern',
  'animated_stripe_pattern_2',
  'global_plus_local_table',
  'global_plus_local_table_with_optim',
  'has_transparency',
  'has_transparency_2',
  'max_color_table_test',
  'min_color_table_test',
  'min_size',
  'noise256',
  'noise6',
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
