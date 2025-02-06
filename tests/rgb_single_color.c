#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  200
#define HEIGHT 200

/* create an image with a single color, save as GIF by using built-in color quantization in cgif_rgb_addframe */
int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  config.path = "rgb_single_color.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_rgb_newgif()\n", stderr);
    return 1;
  }

  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  memset(pImageDataRGB, 128, WIDTH * HEIGHT * 3);  // set all to gray tone [128,128,128]
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
