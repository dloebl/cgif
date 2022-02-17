#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  100
#define HEIGHT 100

static void initGIFConfig(CGIF_Config* pConfig, uint8_t* pGlobalPalette, uint16_t numColors, uint32_t attrFlags, uint16_t width, uint16_t height) {
  memset(pConfig, 0, sizeof(CGIF_Config));
  pConfig->attrFlags               = attrFlags;
  pConfig->width                   = width;
  pConfig->height                  = height;
  pConfig->pGlobalPalette          = pGlobalPalette;
  pConfig->numGlobalPaletteEntries = numColors;
  pConfig->path                   = "all_optim.gif";
}

static void initFrameConfig(CGIF_FrameConfig* pConfig, uint8_t* pImageData, uint16_t delay, uint32_t genFlags) {
  memset(pConfig, 0, sizeof(CGIF_FrameConfig));
  pConfig->pImageData = pImageData;
  pConfig->delay      = delay;
  pConfig->genFlags   = genFlags;
}

int main(void) {
  CGIF*          pGIF;
  CGIF_Config      gConfig;
  CGIF_FrameConfig    fConfig;
  uint8_t*      pImageData;
  uint8_t aPalette[] = {
    0x00, 0x00, 0x00, // black
    0xFF, 0xFF, 0xFF, // white
  };
  cgif_result r;
  uint8_t numColors = 2;   // number of colors in aPalette
  //
  // create new GIF
  initGIFConfig(&gConfig, aPalette, numColors, CGIF_ATTR_IS_ANIMATED, WIDTH, HEIGHT);
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }
  //
  // add frames to GIF
  pImageData = malloc(WIDTH * HEIGHT);         // Actual image data
  memset(pImageData, 0, WIDTH * HEIGHT);
  initFrameConfig(&fConfig, pImageData, 100, CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW);
  cgif_addframe(pGIF, &fConfig); // append the new frame
  memset(pImageData + 7, 1, 41);
  memset(pImageData + 7 + WIDTH, 1, 41);
  memset(pImageData + 7 + WIDTH * 2, 1, 41);
  memset(pImageData + 7 + WIDTH * 2, 0, 29);
  memset(pImageData + 7 + WIDTH * 3, 1, 41);
  memset(pImageData + 7 + WIDTH * 3, 0, 23);
  memset(pImageData + 7 + WIDTH * 4, 0, 37);
  r = cgif_addframe(pGIF, &fConfig); // append the next frame
  //
  free(pImageData);
  //
  // Free allocated space at the end of the session
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
