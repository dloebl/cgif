#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  1000
#define HEIGHT 1000

/* horizontal stripe pattern with 1pixel linewidth, to test interlaced mode */
int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  cgif_result   r;

  uint8_t aPalette[] = {
    0x00, 0xFF, 0x00, // green
    0xFF, 0x00, 0xFF, // purple
  };
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 2;
  gConfig.path                    = "stripe_pattern_interlaced.gif";
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
  for(int i = 0; i < HEIGHT; ++i){
    memset(pImageData + i*WIDTH, i % 2, WIDTH);
  }
  fConfig.pImageData = pImageData;
  fConfig.attrFlags = CGIF_FRAME_ATTR_INTERLACED;
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  r = cgif_close(pGIF); // free allocated space at the end of the session

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
