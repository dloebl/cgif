#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  103
#define HEIGHT 104

/* This is an example code that creates a GIF-animation with a moving stripe-pattern. */
int main(void) {
  CGIF*         pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t aPalette[3 * 256] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  cgif_result r;
  uint16_t numColors = 256;   // number of colors in aPalette
  //
  // Create new GIF
  //
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.attrFlags               = CGIF_ATTR_NO_GLOBAL_TABLE | CGIF_ATTR_IS_ANIMATED;
  gConfig.genFlags                = CGIF_GEN_KEEP_IDENT_FRAMES;
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.path                    = "local_transp.gif";
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
  fConfig.pLocalPalette = aPalette;
  fConfig.numLocalPaletteEntries = numColors;
  fConfig.attrFlags = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConfig.genFlags = CGIF_FRAME_GEN_USE_TRANSPARENCY;
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
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
