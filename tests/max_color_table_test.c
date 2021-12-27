#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

/* This is an example code that creates a GIF-image with all green pixels. */
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t*      aPalette;
  cgif_result   r;
  uint16_t      numColors         = 256; // number of colors in aPalette  
  //
  // create an image
  aPalette   = malloc(256 * 3);
  memset(aPalette, 0, 256 * 3);
  pImageData = malloc(WIDTH * HEIGHT);   // actual image data
  memset(pImageData, 0, WIDTH * HEIGHT);
  //
  // create new GIF
  memset(&gConfig, 0, sizeof(CGIF_Config));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "max_color_table_test.gif";
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // add frames to GIF
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  fConfig.pImageData = pImageData;
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);  
  free(aPalette);
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
