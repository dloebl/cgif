#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif_raw.h"

/*
 * Regression test for integer overflow in LZW buffer allocation (PR #107).
 *
 * The original code computed the LZW data buffer size as:
 *
 *   (numPixel + 2 + maxResets) * sizeof(uint16_t)
 *
 * using uint32_t arithmetic.  When numPixel approaches UINT32_MAX
 * (e.g. a 65535x65535 GIF = ~4.3 billion pixels), the additive expression
 * wraps, producing a drastically undersized malloc followed by a heap
 * buffer overflow during LZW encoding.
 *
 * The fix promotes the arithmetic to size_t and adds explicit overflow
 * guards that return CGIF_EALLOC before calling malloc.
 *
 * This test calls LZW_GenerateStream() directly (via source inclusion)
 * with a numPixel value large enough to trigger the overflow.  A malloc
 * wrapper intercepts the pLZWData allocation and verifies that its size
 * is not truncated.
 *
 * Expected results:
 *   main (unfixed):  pLZWData malloc has a wrapped (tiny) size  => FAIL
 *   fixed:           overflow guard returns CGIF_EALLOC cleanly => PASS
 */

/* ---- malloc interception ---- */

static int   malloc_count;
static int   overflow_detected;
static size_t overflow_alloc_size;

static void* test_malloc(size_t size) {
  ++malloc_count;
  /*
   * Inside LZW_GenerateStream the malloc calls are:
   *   #1  pContext      (small)
   *   #2  pTreeInit     (small)
   *   #3  pTreeList     (small)
   *   #4  pTreeMap      (small)
   *   #5  pLZWData      <-- the one that overflows on main
   *
   * On the fixed branch the overflow guard fires between #4 and #5,
   * so malloc_count never reaches 5.
   *
   * For #5 we check whether the requested size is suspiciously small.
   * The correct size for numPixel ~ 4.3 billion is > 8 GB.
   * A uint32-wrapped size would be < 1 MB.
   * We always return NULL for #5 to prevent the function from trying
   * to LZW-encode our dummy (1-byte) image buffer.
   */
  if (malloc_count == 5) {
    overflow_alloc_size = size;
    if (size < 100000000) { /* < 100 MB => uint32_t overflow occurred */
      overflow_detected = 1;
    }
    return NULL; /* never allocate — image data is a dummy */
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
   * numPixel that triggers uint32_t overflow in (numPixel + 2 + maxResets):
   *
   *   initDictLen    = 4   (for a 1-colour palette)
   *   entriesPerCycle = MAX_DICT_LEN - 4 - 2 = 4090
   *   maxResets       = 4294000000 / 4090 = 1049877
   *   sum             = 4294000000 + 2 + 1049877 = 4295049879  (> UINT32_MAX)
   *   wrapped uint32  = 82583
   *   wrapped malloc  = 82583 * 2 = 165166 bytes   (~161 KB)
   *
   * On 32-bit (with fix):  size_t overflow guard fires   -> CGIF_EALLOC
   * On 64-bit (with fix):  correct size_t addition (8.6 GB) -> malloc #5
   *                         -> our wrapper returns NULL      -> CGIF_EALLOC
   * On main  (no fix):     wrapped uint32 -> malloc #5 with 161 KB size
   *                         -> overflow_detected = 1         -> FAIL
   */
  const uint32_t numPixel    = 4294000000U;
  const uint16_t initDictLen = 4;   /* 1 << (calcInitCodeLen(1) - 1) */
  const uint8_t  initCodeLen = 3;   /* calcInitCodeLen(1) */

  /* Minimal image data — the overflow check fires before encoding. */
  uint8_t imageData[1] = {0};

  malloc_count        = 0;
  overflow_detected   = 0;
  overflow_alloc_size = 0;
  memset(&result, 0, sizeof(result));

  int r = LZW_GenerateStream(&result, numPixel, imageData, initDictLen, initCodeLen);

  if (overflow_detected) {
    fprintf(stderr,
            "FAIL: uint32 overflow in pLZWData allocation "
            "(requested %zu bytes for %u pixels, expected > 8 GB)\n",
            overflow_alloc_size, numPixel);
    return 1;
  }
  if (r != CGIF_EALLOC) {
    fprintf(stderr, "FAIL: expected CGIF_EALLOC, got %d\n", r);
    return 1;
  }
  return 0;
}
