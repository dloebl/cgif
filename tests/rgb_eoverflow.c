#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

/* These dimensions cause fmtChan(4) * width * height to overflow uint32_t.
   4 * 32768 * 32769 = 4,295,098,368 > UINT32_MAX (4,294,967,295).
   Without the overflow fix, this results in a truncated (undersized) heap
   allocation followed by out-of-bounds writes. */
#define WIDTH  32768
#define HEIGHT 32769

/* no-op write callback (avoid creating large output files) */
static int writeFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

int main(void) {
  CGIFrgb_Config      config  = {0};
  CGIFrgb_FrameConfig fconfig = {0};
  CGIFrgb*            pGIF;
  cgif_result         r;

  config.pWriteFn = writeFn;
  config.width    = WIDTH;
  config.height   = HEIGHT;

  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("cgif_rgb_newgif returned NULL\n", stderr);
    return 1;
  }

  /* Provide a small dummy buffer. The library must reject the oversized
     dimensions before attempting to read the full width*height*fmtChan
     bytes from this pointer. */
  uint8_t dummy[4] = {0};
  fconfig.pImageData = dummy;
  fconfig.fmtChan    = CGIF_CHAN_FMT_RGBA;

  r = cgif_rgb_addframe(pGIF, &fconfig);
  if(r != CGIF_EALLOC) {
    fprintf(stderr, "expected CGIF_EALLOC from cgif_rgb_addframe, got %d\n", r);
    cgif_rgb_close(pGIF);
    return 1;
  }

  /* Close may return an error since the GIF has no valid frames. */
  cgif_rgb_close(pGIF);
  return 0;
}
