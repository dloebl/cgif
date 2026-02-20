#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"
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

/* redirect malloc calls inside library sources to our wrapper */
#define malloc(s) cgif_test_malloc(s)
#include "../src/cgif_raw.c"
/* avoid duplicate static function name */
#define calcNextPower2Ex cgif_calcNextPower2Ex
#include "../src/cgif.c"
#undef calcNextPower2Ex
#undef malloc

/* no-op write callback */
static int writeFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

int main(void) {
  CGIF*            pGIF;
  CGIF_Config      gConfig;
  CGIF_FrameConfig fConfig;
  cgif_result      r;
  uint8_t aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  uint8_t aImageData0[WIDTH * HEIGHT];
  uint8_t aImageData1[WIDTH * HEIGHT];
  memset(aImageData0, 0, sizeof(aImageData0));
  memset(aImageData1, 1, sizeof(aImageData1));

  for(fail_after = 1; ; ++fail_after) {
    memset(&gConfig, 0, sizeof(gConfig));
    gConfig.pWriteFn                = writeFn;
    gConfig.width                   = WIDTH;
    gConfig.height                  = HEIGHT;
    gConfig.pGlobalPalette          = aPalette;
    gConfig.numGlobalPaletteEntries = 2;
    gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED;

    malloc_count = 0;

    pGIF = cgif_newgif(&gConfig);
    if(pGIF == NULL) {
      if(malloc_count >= fail_after) {
        continue;
      }
      fputs("unexpected NULL from cgif_newgif\n", stderr);
      return 1;
    }

    // frame 1: local color table + interlaced
    memset(&fConfig, 0, sizeof(fConfig));
    fConfig.pImageData             = aImageData0;
    fConfig.attrFlags              = CGIF_FRAME_ATTR_USE_LOCAL_TABLE | CGIF_FRAME_ATTR_INTERLACED;
    fConfig.pLocalPalette          = aPalette;
    fConfig.numLocalPaletteEntries = 2;
    r = cgif_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= fail_after) {
        cgif_close(pGIF);
        continue;
      }
      fprintf(stderr, "unexpected error from cgif_addframe (frame 1): %d\n", r);
      return 1;
    }

    // frame 2: diff window optimization
    memset(&fConfig, 0, sizeof(fConfig));
    fConfig.pImageData = aImageData1;
    fConfig.genFlags   = CGIF_FRAME_GEN_USE_DIFF_WINDOW;
    r = cgif_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= fail_after) {
        cgif_close(pGIF);
        continue;
      }
      fprintf(stderr, "unexpected error from cgif_addframe (frame 2): %d\n", r);
      return 1;
    }

    // frame 3: transparency optimization (triggers queue flush + separate malloc path)
    memset(&fConfig, 0, sizeof(fConfig));
    fConfig.pImageData = aImageData0;
    fConfig.genFlags   = CGIF_FRAME_GEN_USE_TRANSPARENCY;
    r = cgif_addframe(pGIF, &fConfig);
    if(r != CGIF_OK) {
      if(r == CGIF_EALLOC && malloc_count >= fail_after) {
        cgif_close(pGIF);
        continue;
      }
      fprintf(stderr, "unexpected error from cgif_addframe (frame 3): %d\n", r);
      return 1;
    }

    r = cgif_close(pGIF);
    if(malloc_count < fail_after) {
      // all mallocs succeeded without hitting our failure point -- done
      if(r != CGIF_OK) {
        fprintf(stderr, "unexpected error from cgif_close: %d\n", r);
        return 1;
      }
      break;
    }
    // our injected failure was hit during close (e.g. LZW encoding)
    if(r != CGIF_EALLOC) {
      fprintf(stderr, "expected CGIF_EALLOC from cgif_close, got: %d (fail_after=%d)\n", r, fail_after);
      return 1;
    }
    continue;
  }

  return 0;
}
