tests = [
  'all_optim',
  'alpha',
  'animated_color_gradient',
  'animated_single_pixel',
  'animated_snake',
  'animated_stripes_horizontal',
  'animated_stripe_pattern',
  'animated_stripe_pattern_2',
  'earlyclose',
  'eindex',
  'eindex_anim',
  'ewrite',
  'global_plus_local_table',
  'global_plus_local_table_with_optim',
  'has_transparency',
  'has_transparency_2',
  'local_transp',
  'max_color_table_test',
  'min_color_table_test',
  'min_size',
  'more_than_256_colors',
  'noise256',
  'noise6',
  'noloop',
  'one_full_block',
  'only_local_table',
  'overlap_everything',
  'overlap_everything_only_trans',
  'overlap_some_rows',
  'rgb_255colors',
  'rgb_256colors',
  'rgb_256digit',
  'rgb_all_colors',
  'rgb_single_color',
  'rgb_noise',
  'rgb_noise_animated',
  'trans_inc_initdict',
  'user_trans',
  'write_fn',
]

foreach t : tests
  test_exe = executable(
    'test_' + t,
    t + '.c',
    dependencies : [libcgif_dep],
    include_directories : ['../inc/'],
  )
  test(t, test_exe, priority : 0)
endforeach

md5sumc = find_program('scripts/md5sum.py')
# get the ordering right:
# md5sum check on output GIFs should be run once all of the above tests are done.
test('check md5sums', md5sumc, args : [join_paths(meson.source_root(), 'tests/tests.md5')], workdir : meson.build_root(), priority : -1, is_parallel : false)
