#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

static int pWriteFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

/* regression test: fmtChan * numPixel overflows uint32_t for large dimensions with RGBA.
   on 32-bit platforms size_t is 32 bits, so the overflow check fires and CGIF_EALLOC is returned.
   on 64-bit platforms the multiplication fits in size_t but processing a 4 GB image
   would time out on CI, so we skip. */
int main(void) {
  if(sizeof(size_t) > 4) {
    return 0; // skip on 64-bit: no uint32_t overflow possible in size_t
  }

  const uint16_t width  = 32768;
  const uint16_t height = 32769; // width * height * 4 > UINT32_MAX

  CGIFrgb_Config gConfig = {0};
  gConfig.pWriteFn = pWriteFn;
  gConfig.width    = width;
  gConfig.height   = height;

  CGIFrgb* pGIF = cgif_rgb_newgif(&gConfig);
  if(pGIF == NULL) {
    return 2;
  }

  size_t bufSize = (size_t)width * height * 4;
  uint8_t* buf = malloc(bufSize);
  if(buf == NULL) {
    cgif_rgb_close(pGIF);
    return 0; // skip: not enough memory
  }
  memset(buf, 0, bufSize);

  CGIFrgb_FrameConfig fConfig = {0};
  fConfig.pImageData = buf;
  fConfig.fmtChan    = CGIF_CHAN_FMT_RGBA;
  fConfig.delay      = 0;

  cgif_result r = cgif_rgb_addframe(pGIF, &fConfig);
  cgif_rgb_close(pGIF);
  free(buf);
  if(r == CGIF_EALLOC) {
    return 0; // overflow correctly detected
  }
  return 1;
}
