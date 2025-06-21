#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  4096
#define HEIGHT 4096

/* create an image with colors following a 256digit system covering all possible colors, save as GIF by using built-in color quantization in cgif_rgb_addframe */
int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  config.path = "rgb_all_colors.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_rgb_newgif()\n", stderr);
    return 1;
  }

  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  for(int i = 0; i < WIDTH * HEIGHT; ++i) {
    // fill color according to a digital system
    pImageDataRGB[3*i+2] = i % 256;
    pImageDataRGB[3*i+1] = (i/256) % 256;
    pImageDataRGB[3*i+0] = (i/256/256) % 256;
  }
  fconfig.pImageData = pImageDataRGB;
  fconfig.fmtChan    = CGIF_CHAN_FMT_RGB;
  cgif_rgb_addframe(pGIF, &fconfig);
  cgif_result r = cgif_rgb_close(pGIF);
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  free(pImageDataRGB);
  return 0;
}
