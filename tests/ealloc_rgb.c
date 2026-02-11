#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "cgif.h"
#include "cgif_raw.h"

#define WIDTH  2
#define HEIGHT 2

/* malloc/realloc failure injection */
static int fail_after   = 0;
static int malloc_count = 0;

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

int main(void) {
  CGIFrgb*            pGIF;
  CGIFrgb_Config      gConfig;
  CGIFrgb_FrameConfig fConfig;
  cgif_result         r;
  uint8_t aImageData[] = {
    0xFF, 0x00, 0x00, // red
    0x00, 0xFF, 0x00, // green
    0x00, 0x00, 0xFF, // blue
    0xFF, 0xFF, 0x00, // yellow
  };

  for(int n = 1; ; ++n) {
    memset(&gConfig, 0, sizeof(gConfig));
    memset(&fConfig, 0, sizeof(fConfig));
    gConfig.pWriteFn = writeFn;
    gConfig.width    = WIDTH;
    gConfig.height   = HEIGHT;

    fConfig.pImageData = aImageData;
    fConfig.fmtChan    = CGIF_CHAN_FMT_RGB;

    fail_after  = n;
    malloc_count = 0;

    pGIF = cgif_rgb_newgif(&gConfig);
    if(pGIF == NULL) {
      if(malloc_count >= n) {
        continue; // expected: our injected malloc failure
      }
      fputs("unexpected NULL from cgif_rgb_newgif\n", stderr);
      return 1;
    }

    r = cgif_rgb_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= n) {
        cgif_rgb_close(pGIF);
        continue; // expected: our injected malloc failure
      }
      fprintf(stderr, "unexpected error from cgif_rgb_addframe: %d\n", r);
      return 1;
    }

    r = cgif_rgb_close(pGIF);
    if(malloc_count < n) {
      // all mallocs succeeded without hitting our failure point -- done
      if(r != CGIF_OK) {
        fprintf(stderr, "unexpected error from cgif_rgb_close: %d\n", r);
        return 1;
      }
      break;
    }
    // our injected failure was hit during close
    continue;
  }

  return 0;
}
