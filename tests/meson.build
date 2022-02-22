tests = [
  'all_optim',
  'alpha',
  'animated_color_gradient',
  'animated_interlaced',
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
  'noise6_interlaced',
  'noloop',
  'one_full_block',
  'only_local_table',
  'overlap_everything',
  'overlap_everything_only_trans',
  'overlap_some_rows',
  'stripe_pattern_interlaced',
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
test('check test md5sums', md5sumc, args : [join_paths(meson.source_root(), 'tests/tests.md5')], workdir : meson.build_root(), priority : -1, is_parallel : false)

if get_option('fuzzer')
  zip = find_program('zip', required : false)
  foreach t : tests
    src = [t + '.c', '../fuzz/cgif_create_fuzz_seed.c']
    cflags = ['-D', 'CGIF_OUTPATH="' + t + '.seed' + '"']
    lflags = ['-Wl,--wrap=main']
    exe = executable(t + '_genseed', sources : src, c_args : cflags, link_args: lflags)
    test('generate ' + t + ' fuzz seed', exe, priority : -2)
  endforeach
  # get the ordering right:
  # md5sum check on fuzz seed corpus should be run once all of the above tests are done.
  test('check fuzz seed md5sums', md5sumc, args : [join_paths(meson.source_root(), 'fuzz/seeds.md5')], workdir : meson.build_root(), priority : -3, is_parallel : false)
  if zip.found()
    seeds = []
    foreach t : tests
      seeds += t + '.seed'
    endforeach
    test('create fuzz seed corpus zip archive', zip, args : ['cgif_fuzzer_seed_corpus.zip', seeds], priority : -4, is_parallel : false)
  endif
endif
