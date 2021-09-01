#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

/* This is an example code that creates a GIF-image with all green pixels. */
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = { 0x00, 0xFF, 0x00 }; // green
  uint8_t       numColors  = 1; // number of colors in aPalette  
  //
  // create an image with all green pixels
  pImageData = malloc(WIDTH * HEIGHT);   // actual image data
  memset(pImageData, 0, WIDTH * HEIGHT); // set to all green   
  //
  // create new GIF
  memset(&gConfig, 0, sizeof(CGIF_Config));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "min_color_table_test.gif";
  pGIF = cgif_newgif(&gConfig);  
  //
  // add frames to GIF
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  fConfig.pImageData = pImageData;
  cgif_addframe(pGIF, &fConfig);
  free(pImageData);  
  //
  // free allocated space at the end of the session
  cgif_close(pGIF);  
  return 0;
}
