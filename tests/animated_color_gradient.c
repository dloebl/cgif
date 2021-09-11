#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  256
#define HEIGHT 16

/* This is an example code that creates a GIF-animation with a moving red color gradient with the maximum number of colors (1st and last color is not red)*/
int main(void) {
  CGIF*             pGIF;
  CGIF_Config       gConfig;
  CGIF_FrameConfig  fConfig;
  uint8_t*          pImageData;
  uint8_t           aPalette[3*WIDTH];
  // define red color gradient
  for (int j = 0; j < WIDTH; ++j) {
    aPalette[j*3] = j;
    aPalette[j*3+1] = 0;
    aPalette[j*3+2] = 0;
  }
  // set first color to blue
  aPalette[0] = 0;
  aPalette[1] = 0;
  aPalette[2] = 0xFF;
  // set last color to green
  aPalette[3*(WIDTH-1)] = 0;
  aPalette[3*(WIDTH-1)+1] = 0xFF;
  aPalette[3*(WIDTH-1)+2] = 0;
  
  uint16_t numColors   = (uint16_t)(WIDTH);                // number of colors in aPalette
  int numFrames       = WIDTH;                             // number of frames in the video
  
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED; // set needed attribution flag (as GIF is animated) 
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "animated_color_gradient.gif";
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);         // actual image data
  fConfig.genFlags   = CGIF_FRAME_GEN_USE_DIFF_WINDOW | CGIF_FRAME_GEN_USE_TRANSPARENCY; // it should be automatically detected that transparency optimization is not possible if 256 colors are used
  fConfig.pImageData = pImageData;             // set pointer to image data
  fConfig.delay      = 5;                      // set time before next frame (in units of 0.01 s)
  for (int f = 0; f < numFrames; ++f) {
    for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    	pImageData[i] = (unsigned char)((i + f) % WIDTH); // ceate a moving stripe pattern
    }
    cgif_addframe(pGIF, &fConfig);             // append the new frame
  }
  free(pImageData);
  //
  // write GIF to file
  cgif_close(pGIF);                            // free allocated space at the end of the session
  return 0;
}
