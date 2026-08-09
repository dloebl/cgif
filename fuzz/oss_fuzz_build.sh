#!/bin/bash -eu

# build and install cgif
meson setup -Dfuzzer=true --prefix=$WORK --libdir=lib --default-library=static build
meson install -C build
# run tests:
# This is going to generate the seed corpus from all the tests
meson test -C build

cp "build/fuzz/cgif_fuzzer_seed_corpus.zip" $OUT/.
cp "build/fuzz/cgif_file_fuzzer_seed_corpus.zip" $OUT/.
cp "build/fuzz/cgif_rgb_fuzzer_seed_corpus.zip" $OUT/.

# build cgif's fuzz targets
$CXX $CXXFLAGS -o "$OUT/cgif_fuzzer" -I"$WORK/include" \
  $LIB_FUZZING_ENGINE fuzz/cgif_fuzzer.c "$WORK/lib/libcgif.a"

$CXX $CXXFLAGS -o "$OUT/cgif_file_fuzzer" -I"$WORK/include" \
  $LIB_FUZZING_ENGINE fuzz/cgif_file_fuzzer.c "$WORK/lib/libcgif.a"

$CXX $CXXFLAGS -o "$OUT/cgif_rgb_fuzzer" -I"$WORK/include" \
  $LIB_FUZZING_ENGINE fuzz/cgif_rgb_fuzzer.c "$WORK/lib/libcgif.a"
