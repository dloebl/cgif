#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  99
#define HEIGHT 99

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = {
    0x00, 0x00, 0x00, // black (transparent)
    0xFF, 0xFF, 0xFF, // white
  };

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED | CGIF_ATTR_HAS_TRANSPARENCY;  // first entry in color table is transparency
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 2;
  gConfig.path                    = "has_transparency_2.gif";
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);
  memset(pImageData, 0, WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  fConfig.delay      = 100;
  // create an off/on pattern
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = (i/10) % 2;
  }
  cgif_addframe(pGIF, &fConfig);
  // create opposite pattern
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = 1 - ((i/10) % 2);
  }
  cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  cgif_close(pGIF);
  return 0;
}
