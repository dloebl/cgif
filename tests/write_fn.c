#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

static int pWriteFn(void* pContext, const uint8_t* pData, const size_t numBytes) {
  return fwrite(pData, 1, numBytes, (FILE*) pContext);
}

int main(void) {
  GIF*          pGIF;
  GIFConfig     gConfig;
  FrameConfig   fConfig;
  uint8_t*      pImageData;
  uint8_t       aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };

  FILE* file = fopen("write_fn.gif", "wb");

  memset(&gConfig, 0, sizeof(GIFConfig));
  memset(&fConfig, 0, sizeof(FrameConfig));
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
