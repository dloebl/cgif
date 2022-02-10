#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  255
#define HEIGHT 255 

/* This code is an example of how a GIF with frames including more than 256 colors can be created by reusing colors from a previous frame.
   The general restriction of GIF that only 256 new colors can be introduced in one frame remains of course */
int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint16_t      numColorsLocal = 255;
  uint8_t       aLocalPalette[3*numColorsLocal]; // local palette
  uint8_t       aPalette[3*numColorsLocal]; // global palette (only used for 1st frame here)
  cgif_result r;

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_IS_ANIMATED;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = numColorsLocal;
  gConfig.path                    = "more_than_256_colors.gif";

  // define the global color palette
  for(int i=0; i<numColorsLocal; i++){
    aPalette[3*i+0] = i; // set color palette to red gradient
    aPalette[3*i+1] = 0;
    aPalette[3*i+2] = 0;
  }
  
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }

  pImageData = malloc(WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  fConfig.delay      = 200;

  // 1st frame (uses global palette, but could use local palette, too)
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = i % WIDTH;
  }
  cgif_addframe(pGIF, &fConfig);
  
  // set configs for the folowing frames
  fConfig.numLocalPaletteEntries = numColorsLocal;
  fConfig.pLocalPalette = aLocalPalette;
  fConfig.transIndex = numColorsLocal;
  fConfig.attrFlags = CGIF_FRAME_ATTR_USE_LOCAL_TABLE | CGIF_FRAME_ATTR_HAS_SET_TRANS; // use local palettes, transparency set by user
  
  // 2nd frame
  for(int i=0; i<numColorsLocal; i++){
    aLocalPalette[3*i+0] = 0;
    aLocalPalette[3*i+1] = i; // set color palette to green gradient
    aLocalPalette[3*i+2] = 0;
  }
  memset(pImageData, numColorsLocal, WIDTH * HEIGHT); // set everything to transparent (frame from before shines through)
  for (int i = WIDTH/3 * HEIGHT; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = i % WIDTH;  // set part of the image to new colors
  }
  r = cgif_addframe(pGIF, &fConfig);
  
  // 3rd frame
  for(int i=0; i<numColorsLocal; i++){
    aLocalPalette[3*i+0] = 0;
    aLocalPalette[3*i+1] = 0;
    aLocalPalette[3*i+2] = i; // set color palette to blue gradient
  }
  memset(pImageData, numColorsLocal, WIDTH * HEIGHT); // set everything to transparent (frame from before shines through)
  for (int i = WIDTH/3*2 * HEIGHT; i < (WIDTH * HEIGHT); ++i) {
    pImageData[i] = i % WIDTH;  // set part of the image to new colors
  }
  r = cgif_addframe(pGIF, &fConfig);
  
  free(pImageData);
  
  // write GIF to file
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
