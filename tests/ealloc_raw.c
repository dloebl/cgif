#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif_raw.h"

#define WIDTH  2
#define HEIGHT 2

/* malloc failure injection */
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

/* redirect malloc calls inside cgif_raw.c to our wrapper */
#define malloc(s) cgif_test_malloc(s)
#include "../src/cgif_raw.c"
#undef malloc

/* no-op write callback */
static int writeFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

int main(void) {
  CGIFRaw*            pGIF;
  CGIFRaw_Config      gConfig;
  CGIFRaw_FrameConfig fConfig;
  cgif_result         r;
  uint8_t aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  uint8_t aImageData[WIDTH * HEIGHT];
  memset(aImageData, 0, sizeof(aImageData));

  for(fail_after = 1; ; ++fail_after) {
    memset(&gConfig, 0, sizeof(gConfig));
    memset(&fConfig, 0, sizeof(fConfig));
    gConfig.pWriteFn = writeFn;
    gConfig.width    = WIDTH;
    gConfig.height   = HEIGHT;
    gConfig.pGCT     = aPalette;
    gConfig.sizeGCT  = 2;

    fConfig.pImageData = aImageData;
    fConfig.width      = WIDTH;
    fConfig.height     = HEIGHT;
    fConfig.attrFlags  = CGIF_RAW_FRAME_ATTR_INTERLACED;

    malloc_count = 0;

    pGIF = cgif_raw_newgif(&gConfig);
    if(pGIF == NULL) {
      if(malloc_count >= fail_after) {
        continue; // expected: our injected malloc failure
      }
      fputs("unexpected NULL from cgif_raw_newgif\n", stderr);
      return 1;
    }

    r = cgif_raw_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= fail_after) {
        cgif_raw_close(pGIF);
        continue; // expected: our injected malloc failure
      }
      fprintf(stderr, "unexpected error from cgif_raw_addframe: %d\n", r);
      return 1;
    }

    r = cgif_raw_close(pGIF);
    if(malloc_count < fail_after) {
      // all mallocs succeeded without hitting our failure point -- done
      if(r != CGIF_OK) {
        fprintf(stderr, "unexpected error from cgif_raw_close: %d\n", r);
        return 1;
      }
      break;
    }
    // our injected failure was hit during close (e.g. LZW encoding)
    if(r != CGIF_EALLOC) {
      fprintf(stderr, "expected CGIF_EALLOC from cgif_raw_close, got: %d (n=%d)\n", r, fail_after);
      return 1;
    }
  }

  return 0;
}
