#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "cgif.h"
#include "cgif_raw.h"

/*
 * 18x16 = 288 pixels with >255 unique colors to trigger color quantization.
 * The first 32 colors are crafted to trigger resize_col_hash_table via the
 * collision path (>30 collisions): colors (0,0,0)..(0,0,30) occupy consecutive
 * hash slots, then (8,0,21) hashes to the same slot as (0,0,0) causing 31
 * collisions.
 */
#define WIDTH  18
#define HEIGHT 16

/* malloc/realloc failure injection */
static int fail_after;
static int malloc_count;

static void* cgif_test_malloc(size_t size) {
  if(fail_after > 0) {
    ++malloc_count;
    if(malloc_count == fail_after) {
      return NULL;
    }
  }
  return malloc(size);
}

static void* cgif_test_realloc(void* ptr, size_t size) {
  if(fail_after > 0) {
    ++malloc_count;
    if(malloc_count == fail_after) {
      return NULL;
    }
  }
  return realloc(ptr, size);
}

/* redirect malloc/realloc calls inside library sources to our wrappers */
#define malloc(s) cgif_test_malloc(s)
#define realloc(p, s) cgif_test_realloc(p, s)
#include "../src/cgif_raw.c"
/* avoid duplicate static function name */
#define calcNextPower2Ex cgif_calcNextPower2Ex
#include "../src/cgif.c"
#undef calcNextPower2Ex
#include "../src/cgif_rgb.c"
#undef malloc
#undef realloc

/* no-op write callback */
static int writeFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

static void initImageData(uint8_t* p, int numPixels) {
  int i = 0;
  // first 31 pixels: (0,0,0)..(0,0,30) -- occupy consecutive hash slots
  for(; i < 31 && i < numPixels; ++i) {
    p[3 * i + 0] = 0;
    p[3 * i + 1] = 0;
    p[3 * i + 2] = i;
  }
  // 32nd pixel: (8,0,21) -- hashes to slot 0 (8*65536+21 = 524309 = tableSize),
  // causing 31 collisions which triggers resize_col_hash_table
  if(i < numPixels) {
    p[3 * i + 0] = 8;
    p[3 * i + 1] = 0;
    p[3 * i + 2] = 21;
    ++i;
  }
  // remaining pixels: unique colors starting from (31,0,0)
  for(int c = 31; i < numPixels; ++i, ++c) {
    p[3 * i + 0] = (c >> 0) & 0xFF;
    p[3 * i + 1] = (c >> 8) & 0xFF;
    p[3 * i + 2] = 0;
  }
}

int main(void) {
  CGIFrgb*            pGIF;
  CGIFrgb_Config      gConfig;
  CGIFrgb_FrameConfig fConfig;
  cgif_result         r;
  uint8_t             aImageData[WIDTH * HEIGHT * 3];
  initImageData(aImageData, WIDTH * HEIGHT);

  for(fail_after = 1; ; ++fail_after) {
    memset(&gConfig, 0, sizeof(gConfig));
    memset(&fConfig, 0, sizeof(fConfig));
    gConfig.pWriteFn = writeFn;
    gConfig.width    = WIDTH;
    gConfig.height   = HEIGHT;

    fConfig.pImageData = aImageData;
    fConfig.fmtChan    = CGIF_CHAN_FMT_RGB;
    fConfig.attrFlags  = CGIF_RGB_FRAME_ATTR_INTERLACED;

    malloc_count = 0;

    pGIF = cgif_rgb_newgif(&gConfig);
    if(pGIF == NULL) {
      if(malloc_count >= fail_after) {
        continue; // expected: our injected malloc failure
      }
      fputs("unexpected NULL from cgif_rgb_newgif\n", stderr);
      return 1;
    }

    r = cgif_rgb_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= fail_after) {
        cgif_rgb_close(pGIF);
        continue; // expected: our injected malloc failure
      }
      fprintf(stderr, "unexpected error from cgif_rgb_addframe: %d\n", r);
      return 1;
    }

    r = cgif_rgb_close(pGIF);
    if(malloc_count < fail_after) {
      // all mallocs succeeded without hitting our failure point -- done
      if(r != CGIF_OK) {
        fprintf(stderr, "unexpected error from cgif_rgb_close: %d\n", r);
        return 1;
      }
      break;
    }
    // our injected failure was hit during close (e.g. LZW encoding)
    if(r != CGIF_EALLOC) {
      fprintf(stderr, "expected CGIF_EALLOC from cgif_rgb_close, got: %d (fail_after=%d)\n", r, fail_after);
      return 1;
    }
  }

  return 0;
}
