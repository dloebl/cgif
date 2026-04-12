#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

/*
 * Regression test for 32-bit overflow in MAX_CODE_LEN * lzwPos.
 *
 * In cgif_raw.c the LZW byte-list allocation sizes were computed as:
 *
 *   MAX_CODE_LEN * lzwPos / 8 + ...
 *
 * MAX_CODE_LEN is an int literal (12) and lzwPos is uint32_t.  Under C
 * usual arithmetic conversions the multiplication is performed in uint32_t,
 * which wraps when lzwPos > UINT32_MAX / 12 (~358 million).  The result is
 * a drastically undersized malloc followed by a heap buffer overflow in
 * create_byte_list().
 *
 * The fix casts MAX_CODE_LEN to uint64_t so the multiplication is done in
 * 64-bit arithmetic.
 *
 * This test encodes 361 million pseudo-random pixels (19000 x 19000 with
 * 256 colours).  Random noise defeats LZW compression, giving
 * lzwPos >= numPixel > 358 M and triggering the overflow.
 *
 * Expected results:
 *   main (unfixed):  crash (SIGSEGV) due to heap buffer overflow
 *   fixed:           CGIF_OK or CGIF_EALLOC (clean exit)
 *
 * NOTE: peak memory consumption is approximately 2.5 GB.
 */

#define WIDTH  19000
#define HEIGHT 19000

static uint64_t seed;

// unsigned integer overflow expected
__attribute__((no_sanitize("integer")))
int psdrand(void) {
  // simple pseudo random function from musl libc
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

/* no-op write callback (avoids writing a multi-GB file) */
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
  uint8_t*         pImageData;
  cgif_result      r;
  uint8_t          aPalette[256 * 3];

  seed = 42;
  for(int i = 0; i < 256; ++i) {
    aPalette[i * 3]     = psdrand() % 256;
    aPalette[i * 3 + 1] = psdrand() % 256;
    aPalette[i * 3 + 2] = psdrand() % 256;
  }
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.pWriteFn                = writeFn;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 256;
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // allocate and fill image data with pseudo-random noise
  pImageData = malloc(WIDTH * HEIGHT);
  if(pImageData == NULL) {
    fputs("skipping: insufficient memory (~361 MB required)\n", stderr);
    cgif_close(pGIF);
    return 0; // treat as pass on memory-constrained systems
  }
  for(int i = 0; i < WIDTH * HEIGHT; ++i) pImageData[i] = psdrand() % 256;
  fConfig.pImageData = pImageData;
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // CGIF_EALLOC is acceptable: the system could not allocate enough memory
  // for the internal deep copy, but the overflow guard worked correctly.
  if(r == CGIF_EALLOC) {
    cgif_close(pGIF);
    return 0; // clean allocation failure — pass
  }
  if(r != CGIF_OK) {
    fprintf(stderr, "unexpected error from cgif_addframe: %d\n", r);
    cgif_close(pGIF);
    return 2;
  }
  //
  // close GIF — this triggers LZW encoding and the byte-list allocation
  // where the 32-bit overflow occurs on the unfixed main branch.
  r = cgif_close(pGIF);

  // check for errors
  if(r == CGIF_EALLOC) {
    return 0; // clean allocation failure — pass
  }
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 3;
  }
  return 0;
}
