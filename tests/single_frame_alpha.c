#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH 1
#define HEIGHT 1

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = {
    0xFF, 0xFF, 0xFF, // white
  };
  cgif_result r;

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 1;
  gConfig.path                    = "single_frame_alpha.gif";
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
  memset(pImageData, 1, WIDTH * HEIGHT); // set all transparent
  fConfig.pImageData = pImageData;
  fConfig.attrFlags = CGIF_FRAME_ATTR_HAS_ALPHA;
  fConfig.transIndex = 1;
  cgif_addframe(pGIF, &fConfig);
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
