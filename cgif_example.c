#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  1024
#define HEIGHT 1024

/* Small helper functions to initialize GIF- and frame-configuration */
static void initGIFConfig(CGIF_Config* pConfig, char* path, uint16_t width, uint16_t height, uint8_t* pPalette, uint16_t numColors) {
  memset(pConfig, 0, sizeof(CGIF_Config));
  pConfig->width                   = width;
  pConfig->height                  = height;
  pConfig->pGlobalPalette          = pPalette;
  pConfig->numGlobalPaletteEntries = numColors;
  pConfig->path                    = path;
}
static void initFrameConfig(CGIF_FrameConfig* pConfig, uint8_t* pImageData) {
  memset(pConfig, 0, sizeof(CGIF_FrameConfig));
  pConfig->pImageData = pImageData;
}

/* This is an example code that creates a GIF-image with random pixels. */
int main(void) {
  CGIF*          pGIF;                                          // struct containing the GIF
  CGIF_Config     gConfig;                                        // global configuration parameters for the GIF
  CGIF_FrameConfig   fConfig;                                     // configuration parameters for a frame
  uint8_t*      pImageData;                                     // image data (an array of color-indices)
  uint8_t       aPalette[] = {0xFF, 0x00, 0x00,                 // red
                              0x00, 0xFF, 0x00,                 // green
                              0x00, 0x00, 0xFF};                // blue
  uint16_t numColors = 3;                                        // number of colors in aPalette (up to 256 possible)

  // initialize the GIF-configuration and create a new GIF
  initGIFConfig(&gConfig, "example_cgif.gif", WIDTH, HEIGHT, aPalette, numColors);
  pGIF = cgif_newgif(&gConfig);

  // create image frame with stripe pattern
  pImageData = malloc(WIDTH * HEIGHT);                          // allocate memory for image data
  for (int i = 0; i < (WIDTH * HEIGHT); ++i) {                  // Generate pattern
    pImageData[i] = (unsigned char)((i % WIDTH)/4 % numColors); // stripe pattern (4 pixels per stripe)
  }

  // add frame to GIF
  initFrameConfig(&fConfig, pImageData);                         // initialize the frame-configuration
  cgif_addframe(pGIF, &fConfig);                                 // add a new frame to the GIF
  free(pImageData);                                              // free image data when frame is added

  // close GIF and free allocated space
  cgif_close(pGIF);
  return 0;
}
