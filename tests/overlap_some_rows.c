#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

/* This is an example code that creates a GIF-animation with a moving stripe-pattern. */
int main(void) {
  GIF*         pGIF;
  GIFConfig     gConfig;
  FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  uint8_t numColors = 2;   // number of colors in aPalette
  int numFrames = 2;      // number of frames in the video
  //
  // Create new GIF
  //
  memset(&gConfig, 0, sizeof(GIFConfig));
  memset(&fConfig, 0, sizeof(FrameConfig));
  gConfig.attrFlags               = GIF_ATTR_IS_ANIMATED;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "overlap_some_rows.gif";
  pGIF = cgif_newgif(&gConfig);
  //
  // Add frames to GIF
  //
  pImageData = malloc(WIDTH * HEIGHT);         // Actual image data
  memset(pImageData, 0, WIDTH * HEIGHT); 
  fConfig.pImageData = pImageData;
  fConfig.delay      = 100;  // set time before next frame (in units of 0.01 s)
  fConfig.genFlags   = FRAME_GEN_USE_DIFF_WINDOW;
  cgif_addframe(pGIF, &fConfig); // append the new frame
  memset(pImageData + 7, 1, 55);
  memset(pImageData + 7 + WIDTH, 1, 55);
  memset(pImageData + 7 + WIDTH * 2, 1, 55);
  memset(pImageData + 7 + WIDTH * 3, 1, 55);
  cgif_addframe(pGIF, &fConfig); // append the next frame
  //
  free(pImageData);
  //
  // Free allocated space at the end of the session
  //
  cgif_close(pGIF);
  return 0;
}
