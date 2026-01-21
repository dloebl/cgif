#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH 3840  // 4096 - 2 - 256 = 3838 (free lzw codes)  +1 (to force dict reset: k pixels make adding k-1 codes)  +1 to overflow if too litte is allocated in pLZWData
#define HEIGHT 1

static uint64_t seed;

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
  int palette_size = 256;
  uint8_t       aPalette[palette_size * 3]; 

  seed = 42;
  for(int i = 0; i < palette_size; ++i) {
    aPalette[i * 3]     = psdrand() % 256;
    aPalette[i * 3 + 1] = psdrand() % 256;
    aPalette[i * 3 + 2] = psdrand() % 256;
  }
  memset(&gConfig, 0, sizeof(CGIF_Config));
  memset(&fConfig, 0, sizeof(CGIF_FrameConfig));
  gConfig.width                   = WIDTH;
  gConfig.height                  = HEIGHT;
  gConfig.pGlobalPalette          = aPalette;
  gConfig.numGlobalPaletteEntries = palette_size;
  gConfig.path                    = "avoid_compression.gif";
  //
  // create new GIF
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // add frames to GIF
  uint8_t* code_dict = malloc(sizeof(uint8_t)*palette_size*palette_size); // to check if codes are used already, then avoid them
  memset(code_dict, 0, sizeof(uint8_t)*palette_size*palette_size);
  pImageData = malloc(WIDTH * HEIGHT);
  pImageData[0] = psdrand() % palette_size;
  for(int i = 1; i < WIDTH * HEIGHT; ++i){
    uint8_t idx = 0;
    while (idx == 0 || code_dict[palette_size*idx + pImageData[i-1]]) idx = psdrand() % palette_size;
    pImageData[i] = idx;
	code_dict[palette_size*idx + pImageData[i-1]] = 1;  //mark code as used
  }
  fConfig.pImageData = pImageData;
  r = cgif_addframe(pGIF, &fConfig);
  free(code_dict);
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
