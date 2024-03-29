#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  500
#define HEIGHT 500

static uint64_t seed;

// unsigned integer overflow expected
__attribute__((no_sanitize("integer")))
int psdrand(void) {
  // simple pseudo random function from musl libc
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

int main(void) {
  CGIF*          pGIF;
  CGIF_Config     gConfig;
  CGIF_FrameConfig   fConfig;
  uint8_t*      pImageData;
  cgif_result   r;
  uint8_t       aPalette[256 * 3]; 

  seed = 22;
  for(int i = 0; i < 256; ++i) {
    aPalette[i * 3]     = psdrand() % 256;
    aPalette[i * 3 + 1] = psdrand() % 256;
    aPalette[i * 3 + 2] = psdrand() % 256;
  }
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = 256;
  gConfig.path                    = "noise256.gif";
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
  for(int i = 0; i < WIDTH * HEIGHT; ++i) pImageData[i] = psdrand() % 256;
  fConfig.pImageData = pImageData;
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
