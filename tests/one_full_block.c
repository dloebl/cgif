#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  1 
#define HEIGHT 224 // ceil((224 + 2) * 9bit)  =  255byte  (+2 for start- and clear-code)

/* This test creates a GIF-frame with exactly 255 LZW bytes */
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t*      aPalette = malloc(3 * HEIGHT);
  cgif_result   r;
  uint16_t      numColors  = HEIGHT; // number of colors in aPalette  
  
  memset(aPalette, 0, 3 * HEIGHT);
  //
  // create an image
  pImageData = malloc(WIDTH * HEIGHT);   // actual image data
  for(int i = 0; i < HEIGHT; ++i) pImageData[i] = i;
  //
  // create new GIF
  memset(&gConfig, 0, sizeof(CGIF_Config));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path                    = "one_full_block.gif";
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
  free(aPalette);
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
