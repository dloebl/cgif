#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  110
#define HEIGHT 125

/* This is an example code that creates a GIF-animation with a moving stripe-pattern. */
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t aLocalPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  uint8_t aGlobalPalette[] = {
    0x00, 0xFF, 0x00, // green
    0x00, 0x00, 0xFF, // blue
    0xFF, 0x00, 0x00, // red
  };
  cgif_result r;
  uint8_t numColorsLocal = 2;   // number of colors in aPalette
  uint8_t numColorsGlobal = 3;   // number of colors in aPalette
  //
  // Create new GIF
  //
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.pGlobalPalette               = aGlobalPalette;
  gConfig.numGlobalPaletteEntries = numColorsGlobal;
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.path                    = "global_plus_local_table.gif";
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // Add frame to GIF
  //
  pImageData = malloc(WIDTH * HEIGHT);         // Actual image data
  memset(pImageData, 1, WIDTH * HEIGHT);
  memset(pImageData + WIDTH * 2, 0, WIDTH * 3); 
  fConfig.pImageData = pImageData;
  fConfig.pLocalPalette = aLocalPalette;
  fConfig.numLocalPaletteEntries = numColorsLocal;
  fConfig.attrFlags = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConfig.delay = 100;
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
  //
  // add next frame
  fConfig.attrFlags = 0;
  fConfig.genFlags = 0;
  memset(pImageData + WIDTH * 9, 2, WIDTH * 10);
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
  //
  free(pImageData);
  //
  // Free allocated space at the end of the session
  //
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
