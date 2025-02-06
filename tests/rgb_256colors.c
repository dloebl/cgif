#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  256
#define HEIGHT 10

/* create an image with 256 different colors, save as GIF by using cgif_rgb_addframe
   color quantization is done as it keeps index free for transparency in an animation (but it is not required by the GIF-format for exactly 256 colors) */
int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  config.path = "rgb_256colors.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_rgb_newgif()\n", stderr);
    return 1;
  }

  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  memset(pImageDataRGB, 0, WIDTH * HEIGHT * 3); // initialize all rgb-values to [0,0,0]
  for(int i=0; i<WIDTH * HEIGHT; i++){
    pImageDataRGB[3*i] = i % WIDTH; // red color gradient
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
