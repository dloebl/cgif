# some unit tests are expected to fail when we generate the fuzzer seed corpus.
# these tests might fail, as the cgif API wrapper (fuzz/cgif_create_fuzz_seed.c) does not behave exactly as cgif itself:
# seed_should_fail should be set to true if that's the case.

# tests for the indexed API
tests_index = [
  { 'name' : 'all_optim',                          'seed_should_fail' : false},
  { 'name' : 'alpha',                              'seed_should_fail' : false},
  { 'name' : 'animated_color_gradient',            'seed_should_fail' : false},
  { 'name' : 'animated_interlaced',                'seed_should_fail' : false},
  { 'name' : 'animated_single_pixel',              'seed_should_fail' : false},
  { 'name' : 'animated_snake',                     'seed_should_fail' : false},
  { 'name' : 'animated_stripes_horizontal',        'seed_should_fail' : false},
  { 'name' : 'animated_stripe_pattern',            'seed_should_fail' : false},
  { 'name' : 'avoid_compression',                  'seed_should_fail' : false},
  { 'name' : 'animated_stripe_pattern_2',          'seed_should_fail' : false},
  { 'name' : 'duplicate_frames',                   'seed_should_fail' : false},
  { 'name' : 'earlyclose',                         'seed_should_fail' : true },
  { 'name' : 'eindex',                             'seed_should_fail' : true },
  { 'name' : 'eindex_anim',                        'seed_should_fail' : true },
  { 'name' : 'enopalette',                         'seed_should_fail' : true },
  { 'name' : 'ewrite',                             'seed_should_fail' : true },
  { 'name' : 'ezeroheight',                        'seed_should_fail' : true},
  { 'name' : 'ezerowidth',                         'seed_should_fail' : true},
  { 'name' : 'ezerowidthheight',                   'seed_should_fail' : true},
  { 'name' : 'global_plus_local_table',            'seed_should_fail' : false},
  { 'name' : 'global_plus_local_table_with_optim', 'seed_should_fail' : false},
  { 'name' : 'has_transparency',                   'seed_should_fail' : false},
  { 'name' : 'has_transparency_2',                 'seed_should_fail' : false},
  { 'name' : 'local_transp',                       'seed_should_fail' : false},
  { 'name' : 'max_color_table_test',               'seed_should_fail' : false},
  { 'name' : 'min_color_table_test',               'seed_should_fail' : false},
  { 'name' : 'min_size',                           'seed_should_fail' : false},
  { 'name' : 'more_than_256_colors',               'seed_should_fail' : false},
  { 'name' : 'noise256',                           'seed_should_fail' : false},
  { 'name' : 'noise256_large',                     'seed_should_fail' : false},
  { 'name' : 'noise6',                             'seed_should_fail' : false},
  { 'name' : 'noise6_interlaced',                  'seed_should_fail' : false},
  { 'name' : 'noloop',                             'seed_should_fail' : false},
  { 'name' : 'one_full_block',                     'seed_should_fail' : false},
  { 'name' : 'only_local_table',                   'seed_should_fail' : false},
  { 'name' : 'overlap_everything',                 'seed_should_fail' : false},
  { 'name' : 'overlap_everything_only_trans',      'seed_should_fail' : false},
  { 'name' : 'overlap_some_rows',                  'seed_should_fail' : false},
  { 'name' : 'single_frame_alpha',                 'seed_should_fail' : false},
  { 'name' : 'stripe_pattern_interlaced',          'seed_should_fail' : false},
  { 'name' : 'switchpattern',                      'seed_should_fail' : false},
  { 'name' : 'trans_inc_initdict',                 'seed_should_fail' : false},
  { 'name' : 'user_trans_diff_area',               'seed_should_fail' : false},
  { 'name' : 'user_trans_merge',                   'seed_should_fail' : false},
  { 'name' : 'user_trans',                         'seed_should_fail' : false},
  { 'name' : 'write_fn',                           'seed_should_fail' : false},
]

# tests for the rgb API
tests_rgb = [
  { 'name' : 'rgb_255colors',                      'seed_should_fail' : false},
  { 'name' : 'rgb_256colors',                      'seed_should_fail' : false},
  { 'name' : 'rgb_256digit',                       'seed_should_fail' : false},
  { 'name' : 'rgb_interlaced',                     'seed_should_fail' : false},
  { 'name' : 'rgb_single_color',                   'seed_should_fail' : false},
  { 'name' : 'rgb_no_dithering',                   'seed_should_fail' : false},
  { 'name' : 'rgb_noise',                          'seed_should_fail' : false},
  { 'name' : 'rgb_noise_animated',                 'seed_should_fail' : false},
]

foreach t : tests_index + tests_rgb
  name = t.get('name')
  test_exe = executable(
    'test_' + name,
    name + '.c',
    dependencies : [libcgif_dep],
    include_directories : ['../inc/'],
  )
  test(name, test_exe, priority : 0)
endforeach

sha256sumc = find_program('scripts/sha256sum.py')
# get the ordering right:
# md5sum check on output GIFs should be run once all of the above tests are done.
test('check test checksums', sha256sumc, args : [join_paths(meson.project_source_root(), 'tests/tests.sha256')], workdir : meson.project_build_root(), priority : -1, is_parallel : false)
