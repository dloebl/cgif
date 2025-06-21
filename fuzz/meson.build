# TBD: replace with custom python zip tool (just like scripts/sha256sum.py)
# TBD: allow to generate the seed corpus without having to invoke all unit tests.
zip = find_program('zip', required : false)
# create seeds for indexed API
foreach t : tests_index
  name = t.get('name')
  src = [join_paths(meson.project_source_root(), 'tests', name + '.c'), 'cgif_create_fuzz_seed.c']
  cflags = ['-D', 'CGIF_OUTPATH="' + name + '.seed' + '"']
  exe = executable(name + '_genseed', sources : src, c_args : cflags, include_directories : ['../inc'])
  test('generate ' + name + '.seed', exe, workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -2, should_fail : t.get('seed_should_fail'))
endforeach
# create seeds for rgb API
foreach t : tests_rgb
  name = t.get('name')
  src = [join_paths(meson.project_source_root(), 'tests', name + '.c'), 'cgif_rgb_create_fuzz_seed.c']
  cflags = ['-D', 'CGIF_OUTPATH="' + name + '.seed' + '"']
  exe = executable(name + '_genseed', sources : src, c_args : cflags, include_directories : ['../inc'])
  test('generate ' + name + '.seed', exe, workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -2, should_fail : t.get('seed_should_fail'))
endforeach
# get the ordering right:
# sha256sum check on fuzz seed corpus should be run once all of the above tests are done.
test('check fuzz seed checksums', sha256sumc, args : [join_paths(meson.project_source_root(), 'fuzz/seeds.sha256')], workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -3, is_parallel : false)

src_index = ['cgif_fuzzer_standalone.c', 'cgif_fuzzer.c']
exe_index = executable(
  'cgif_fuzzer_standalone',
  sources : src_index,
  dependencies : [libcgif_dep],
  include_directories : ['../inc'],
)

src = ['cgif_fuzzer_standalone.c', 'cgif_file_fuzzer.c']
exe_file = executable(
  'cgif_file_fuzzer_standalone',
  sources : src,
  dependencies : [libcgif_dep],
  include_directories : ['../inc'],
)

src_rgb = ['cgif_fuzzer_standalone.c', 'cgif_rgb_fuzzer.c']
exe_rgb = executable(
  'cgif_rgb_fuzzer_standalone',
  sources : src_rgb,
  dependencies : [libcgif_dep],
  include_directories : ['../inc'],
)
# test indexed seed corpus with standalone fuzzer and standalone file fuzzer
foreach t : tests_index
  seedpath = t.get('name') + '.seed'
  test('run cgif_fuzzer_standalone ' + seedpath, exe_index, args : seedpath, workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -4)
  test('run cgif_file_fuzzer_standalone ' + seedpath, exe_file, args : seedpath, workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -4)
endforeach

# test rgb seed corpus with standalone fuzzer
foreach t : tests_rgb
  seedpath = t.get('name') + '.seed'
  test('run cgif_rgb_fuzzer_standalone ' + seedpath, exe_rgb, args : seedpath, workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -4)
endforeach

# generate seed corpus zip archive, if possible
if zip.found()
  seeds = []
  foreach t : tests_index + tests_rgb
    seeds += t.get('name') + '.seed'
  endforeach
  test('create fuzzer seed corpus zip archive', zip, args : ['cgif_fuzzer_seed_corpus.zip', seeds], workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -4)
  test('create file fuzzer seed corpus zip archive', zip, args : ['cgif_file_fuzzer_seed_corpus.zip', seeds], workdir : join_paths(meson.project_build_root(), 'fuzz'), priority : -4)
endif
