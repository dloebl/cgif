#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  200
#define HEIGHT 200

static uint64_t seed;

static int psdrand(void) {
  // simple pseudo random function from musl libc
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

/* an example of a GIF-animation with random colors:
   GIFs can only have 256 colors per frame. If you have an image with more colors, you can use cgif_rgb_addframe.
   Cgif approximates (quantizes) the colors such that the image has just 256 colors but is visually close to identical to the original.
   Note: color quantization is lossy. If you want to have precise control over every single pixel of the image, use cgif_example.c */
int main(void) {
  CGIFrgb_Config config = {0};
  CGIFrgb*       pGIF;
  CGIFrgb_FrameConfig fconfig = {0};

  seed = 22;
  config.path = "cgif_rgb_example.gif";
  config.width = WIDTH;
  config.height = HEIGHT;
  pGIF = cgif_rgb_newgif(&config);          // optional: check pGIF == NULL for error handling

  uint8_t* pImageDataRGB = malloc(WIDTH * HEIGHT * 3);
  for(int i = 0; i < WIDTH * HEIGHT * 3; ++i) {
    pImageDataRGB[i] = psdrand() % 256;     // choose random color for r, g, b value respectively
  }
  fconfig.pImageData = pImageDataRGB;
  fconfig.fmtChan    = CGIF_CHAN_FMT_RGB;   // fconfig.pImageData comes in RGB (3 byte blocks), not RGBA (4 byte blocks)
  cgif_rgb_addframe(pGIF, &fconfig);        // add frame data (colors are approximated if there are more than 256 different colors in the frame)
  cgif_result r = cgif_rgb_close(pGIF);     // optional: r can be used for error handling

  free(pImageDataRGB);
  return 0;
}
