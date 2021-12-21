#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

static int pWriteFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  size_t r = fwrite(pData, 1, numBytes, (FILE*) pContext);
  if(r == numBytes) {
    return 0;
  } else {
    return -1;
  }
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

  FILE* file = fopen("write_fn.gif", "wb");

  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 2;
  gConfig.pWriteFn                = pWriteFn;
  gConfig.pContext                = (void*) file;
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);
  memset(pImageData, 0, WIDTH * HEIGHT);
  fConfig.pImageData = pImageData;
  cgif_addframe(pGIF, &fConfig);
  free(pImageData);
  //
  // write GIF to file
  cgif_close(pGIF);                  // free allocated space at the end of the session
  fclose(file);
  return 0;
}
