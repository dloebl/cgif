#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  1
#define HEIGHT 1

static int pWriteFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  // just ignore GIF data
  return 0;
}

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  cgif_result r;

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = NULL;
  gConfig.numGlobalPaletteEntries = 4; // set the number global palette entries to an invalid value
  gConfig.pWriteFn                = pWriteFn;
  gConfig.pContext                = NULL;
  gConfig.attrFlags               = CGIF_ATTR_NO_GLOBAL_TABLE | CGIF_ATTR_IS_ANIMATED;
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
  r = cgif_addframe(pGIF, &fConfig);
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  r = cgif_close(pGIF); // free allocated space at the end of the session

  // check for correct error: CGIF_ERROR
  // error is expected, because we don't provide any color palette.
  if(r == CGIF_ERROR) {
    return 0;
  }
  fputs("CGIF_ERROR expected as result code\n", stderr);
  return 2;
}
