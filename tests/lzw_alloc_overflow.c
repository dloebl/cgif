#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif_raw.h"

/*
 * Regression test for integer overflow in the LZW code-buffer allocation.
 *
 * LZW_GenerateStream() sizes pLZWData from:
 *
 *   (numPixel + 2 + maxResets) * sizeof(uint16_t)
 *
 * On a 32-bit target size_t is 32 bits wide, so for a numPixel close to
 * UINT32_MAX (e.g. a 65535x65535 GIF is ~4.3 billion pixels) the additive
 * expression wraps and malloc receives a drastically undersized request.
 * LZW encoding then writes past the end of that buffer.
 *
 * The fix promotes the arithmetic to size_t and returns CGIF_EALLOC via the
 * overflow guard before malloc is ever called.
 *
 * We reach the static function by including the translation unit directly and
 * redirecting its malloc (same approach as ealloc_raw.c).
 *
 * Detecting the buffer without depending on malloc call order:
 * pLZWData is the only allocation that scales with numPixel. Every other
 * allocation is bounded by MAX_DICT_LEN and stays well under 64 KiB for the
 * inputs used here, so we treat any request larger than that threshold as the
 * LZW buffer. We record its size and return NULL so encoding never runs on a
 * dummy image buffer.
 *
 *   main (unfixed), 32-bit:  request wraps to ~161 KiB (< numPixel bytes) -> FAIL
 *   fixed, 32-bit:           guard returns CGIF_EALLOC, buffer never sized -> PASS
 *   64-bit (either):         no wrap, request is > 8 GiB, we force NULL     -> PASS
 */

#define LZW_ALLOC_THRESHOLD 65536 /* every non-LZW allocation stays below this */

static size_t lzwAllocSize; /* size of the LZW-buffer request, 0 if none seen */

static void* test_malloc(size_t size) {
  if(size > LZW_ALLOC_THRESHOLD) {
    lzwAllocSize = size;
    return NULL; /* abort before encoding a dummy image */
  }
  return malloc(size);
}

/* redirect malloc calls inside cgif_raw.c to our wrapper */
#define malloc(s) test_malloc(s)
#include "../src/cgif_raw.c"
#undef malloc

int main(void) {
  LZWResult result;
  /*
   * numPixel large enough to overflow (numPixel + 2 + maxResets) in 32-bit
   * size_t arithmetic. A 65535x65535 frame produces a value in this range.
   */
  const uint32_t numPixel    = 4294000000U;
  const uint16_t initDictLen = 4; /* 1-color palette */
  const uint8_t  initCodeLen = 3;

  uint8_t imageData[1] = {0};

  lzwAllocSize = 0;
  memset(&result, 0, sizeof(result));

  int r = LZW_GenerateStream(&result, numPixel, imageData, initDictLen, initCodeLen);

  if(lzwAllocSize != 0 && lzwAllocSize < (size_t)numPixel) {
    fprintf(stderr,
            "FAIL: LZW buffer request wrapped (%zu bytes for %u pixels)\n",
            lzwAllocSize, numPixel);
    return 1;
  }
  if(r != CGIF_EALLOC) {
    fprintf(stderr, "FAIL: expected CGIF_EALLOC, got %d\n", r);
    return 1;
  }
  return 0;
}
