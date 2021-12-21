#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  40
#define HEIGHT 40

/* Small helper functions to initialize GIF- and frame-configuration */
static void initGIFConfig(CGIF_Config* pConfig, char* path, uint16_t width, uint16_t height, uint8_t* pPalette, uint16_t numColors) {
  memset(pConfig, 0, sizeof(CGIF_Config));
  pConfig->width                   = width;
  pConfig->height                  = height;
  pConfig->pGlobalPalette          = pPalette;
  pConfig->numGlobalPaletteEntries = numColors;
  pConfig->path                    = path;
  pConfig->attrFlags               = CGIF_ATTR_IS_ANIMATED;   
}
static void initFrameConfig(CGIF_FrameConfig* pConfig, uint8_t* pImageData, uint16_t delay) {
  memset(pConfig, 0, sizeof(CGIF_FrameConfig));
  pConfig->delay      = delay;
  pConfig->pImageData = pImageData;
  pConfig->genFlags = CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW; // use optimizations
}

/* This is an example code that creates a GIF-animation with global and local color table, also using the main optimizations to reduce the GIF size */
int main(void) {
  CGIF*        pGIF;                                                      // struct containing the GIF
  CGIF_Config    gConfig;                                                 // global configuration parameters for the GIF
  CGIF_FrameConfig  fConfig;                                              // configuration parameters for a frame
  uint8_t*    pImageData;                                                 // image data (an array of color-indices)
  int f;                                                                  // frame number
  uint8_t aPalette[] = {0xFF, 0x00, 0x00,                                 // red
                        0x00, 0xFF, 0x00,                                 // green
                        0x00, 0x00, 0xFF};                                // blue
  uint8_t aPalette_local[] = {0xFF, 0x00, 0x00,                           // red
                        0x00, 0xFF, 0x00,                                 // green
                        0x00, 0x00, 0x00};                                // black
  cgif_result r;
  uint8_t numColors_global = 3;                                           // number of colors in global color palette (up to 256 possible)
  uint8_t numColors_local = 3;                                            // number of colors in local color palette (up to 256 possible)    
  int numFrames = 10;                                                     // number of frames in the animation

  // initialize the GIF-configuration and create a new GIF
  initGIFConfig(&gConfig, "global_plus_local_table_with_optim.gif", WIDTH, HEIGHT, aPalette, numColors_global);
  pGIF = cgif_newgif(&gConfig);
  if(pGIF == NULL) {
    fputs("failed to create new GIF via cgif_newgif()\n", stderr);
    return 1;
  }

  // create image frames and add them to GIF (use global color table first)
  pImageData = malloc(WIDTH * HEIGHT);                                    // allocate memory for image data
  memset(pImageData, 0, WIDTH * HEIGHT);                                  // set the background color to 1st color in global palette
  for (f = 0; f < numFrames-2; ++f) {
    pImageData[f + WIDTH * (HEIGHT/2)] = 1;
    initFrameConfig(&fConfig, pImageData, 20);                            // initialize the frame-configuration (3rd parameter is the delay in 0.01 s)
    r = cgif_addframe(pGIF, &fConfig);                                    // append the new frame
    if(r != CGIF_OK) {
      break;
    }
  }
  
  // add frame with local color table (one additional black pixel)
  pImageData[WIDTH * (HEIGHT/2) + f] = 2;
  fConfig.pImageData = pImageData;
  fConfig.pLocalPalette = aPalette_local;
  fConfig.numLocalPaletteEntries = numColors_local;
  fConfig.attrFlags = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConfig.delay = 200;                                                    // delay in 0.01 s
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
  
  // add another frame with local color table
  pImageData[WIDTH * (HEIGHT/2) + f + 2] = 1;
  pImageData[WIDTH * (HEIGHT/2) + f + 4] = 1;
  pImageData[WIDTH * (HEIGHT/2) + f + 6] = 1;
  pImageData[WIDTH * (HEIGHT/2) + f + 8] = 1;
  r = cgif_addframe(pGIF, &fConfig); // append the new frame
    
  // refresh pattern and go on with global color table  
  memset(pImageData, 0, WIDTH * HEIGHT);                                  // set the background color to 1st color in global palette
  for (f = 0; f < numFrames-2; ++f) {
    pImageData[2*f + WIDTH * (HEIGHT/2)] = 2;
    initFrameConfig(&fConfig, pImageData, 50);                            // initialize the frame-configuration (3rd parameter is the delay in 0.01 s)
    r = cgif_addframe(pGIF, &fConfig);                                    // append the new frame
    if(r != CGIF_OK) {
      break;
    }
  }
  
  free(pImageData);                                                      // free image data when all frames are added

  // close created GIF-file and free allocated space
  r = cgif_close(pGIF);

  // check for errors
  if(r != CGIF_OK) {
    fprintf(stderr, "failed to create GIF. error code: %d\n", r);
    return 2;
  }
  return 0;
}
