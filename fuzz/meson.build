# TBD: replace with custom python zip tool (just like scripts/md5sum.py)
zip = find_program('zip', required : false)
foreach t : tests
  name = t.get('name')
  src = [join_paths(meson.source_root(), 'tests', name + '.c'), 'cgif_create_fuzz_seed.c']
  cflags = ['-D', 'CGIF_OUTPATH="' + name + '.seed' + '"']
  exe = executable(name + '_genseed', sources : src, c_args : cflags, include_directories : ['../inc'])
  test('generate ' + name + '.seed', exe, workdir : meson.build_root(), priority : -2, should_fail : t.get('seed_should_fail'))
endforeach
# get the ordering right:
# md5sum check on fuzz seed corpus should be run once all of the above tests are done.
test('check fuzz seed checksums', md5sumc, args : [join_paths(meson.source_root(), 'fuzz/seeds.md5')], workdir : meson.build_root(), priority : -3, is_parallel : false)

src = ['cgif_fuzzer_standalone.c', 'cgif_fuzzer.c']
exe = executable(
  'cgif_fuzzer_standalone',
  sources : src,
  dependencies : [libcgif_dep],
  include_directories : ['../inc'],
)
# test seed corpus with standalone fuzzer
foreach t : tests
  seedpath = t.get('name') + '.seed'
  test('run cgif_fuzzer_standalone ' + seedpath, exe, args : seedpath, workdir : meson.build_root(), priority : -4)
endforeach

# generate seed corpus zip archive, if possible
if zip.found()
  seeds = []
  foreach t : tests
    seeds += t.get('name') + '.seed'
  endforeach
  test('create fuzz seed corpus zip archive', zip, args : ['cgif_fuzzer_seed_corpus.zip', seeds], workdir : meson.build_root(), priority : -4)
endif