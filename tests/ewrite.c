#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

static int pWriteFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return -1; // return error code -1: should lead to EWRITE
}

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  cgif_result r;

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 2;
  gConfig.pWriteFn                = pWriteFn;
  gConfig.pContext                = NULL;
  //
  // create new GIF (might call pWriteFn as well)
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    return 0;
  }
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);
  memset(pImageData, 0, WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  r = cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  r = cgif_close(pGIF);                  // free allocated space at the end of the session

  // check for errors
  if(r == CGIF_EWRITE) {
    return 0;
  }
  fputs("CGIF_EWRITE expected as result code\n", stderr);
  return 1;
}
