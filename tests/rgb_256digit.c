#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  1000
#define HEIGHT 1000

/* create an image with colors following a 256digit system, save as GIF by using built-in color quantization in cgif_rgb_addframe */
int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  config.path = "rgb_256digit.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_rgb_newgif()\n", stderr);
    return 1;
  }
  
  int p = 524309; // set to initial size of the hash table + x
  int imod;
  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  for(int i = 0; i < WIDTH * HEIGHT; ++i) {
    if(i<100000){
      imod = i;
    } else {
      imod = p;   // open addressing with linear probing gets inefficient here
    }
    // fill color according to a digital system
    pImageDataRGB[3*i+2] = imod % 256;
    pImageDataRGB[3*i+1] = (imod/256) % 256;
    pImageDataRGB[3*i+0] = (imod/256/256) % 256;
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
