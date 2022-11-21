#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

/* This is an example code that creates a GIF-animation with two identical frames in sequence*/
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  uint8_t numColors = 2;   // number of colors in aPalette
  cgif_result r;
  //
  // Create new GIF
  //
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "duplicate_frames.gif";
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // Add frames to GIF
  //
  pImageData = malloc(WIDTH * HEIGHT);         // Actual image data
  memset(pImageData, 0, WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  fConfig.delay      = 100;  // set time before next frame (in units of 0.01 s)
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
  r = cgif_addframe(pGIF, &fConfig); // append the next frame
  r = cgif_addframe(pGIF, &fConfig); // append the next frame
  r = cgif_addframe(pGIF, &fConfig); // append the next frame
  r = cgif_addframe(pGIF, &fConfig); // append the next frame
  //
  free(pImageData);
  //
  // free allocated space at the end of the session
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
