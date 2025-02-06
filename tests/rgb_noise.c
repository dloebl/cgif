#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  700
#define HEIGHT 320

static uint64_t seed;

static int psdrand(void) {
  // simple pseudo random function from musl libc
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  seed = 22;
  config.path = "rgb_noise.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_rgb_newgif()\n", stderr);
    return 1;
  }
  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  for(int i = 0; i < WIDTH * HEIGHT * 3; ++i) {
    pImageDataRGB[i] = psdrand() % 256;
  }
  fconfig.pImageData = pImageDataRGB;
  fconfig.fmtChan    = CGIF_CHAN_FMT_RGB;
    fconfig.attrFlags  = CGIF_RGB_FRAME_ATTR_NO_DITHERING;
  cgif_rgb_addframe(pGIF, &fconfig);
  cgif_result r = cgif_rgb_close(pGIF);
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  free(pImageDataRGB);
  return 0;
}
