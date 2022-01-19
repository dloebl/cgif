#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  99
#define HEIGHT 99

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = {
    0xFF, 0x00, 0x00, // green 
    0xFF, 0xFF, 0xFF, // white
  };
  cgif_result r;

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 2;
  gConfig.path                    = "alpha.gif";
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);
  memset(pImageData, 0, WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  fConfig.delay      = 100;
  // create an off/on pattern
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = i % 2;
  }
  cgif_addframe(pGIF, &fConfig);
  // create transparent frame (alpha channel)
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = 2;
  }
  fConfig.attrFlags  = CGIF_FRAME_ATTR_HAS_ALPHA;
  fConfig.transIndex = 2;
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
