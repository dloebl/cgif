#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cgif.h"

#define WIDTH  40
#define HEIGHT 40

/* Small helper functions to initialize GIF- and frame-configuration */
static void initGIFConfig(GIFConfig* pConfig, char* path, uint16_t width, uint16_t height, uint8_t* pPalette, uint16_t numColors) {
  memset(pConfig, 0, sizeof(GIFConfig));
  pConfig->width                   = width;
  pConfig->height                  = height;
  pConfig->pGlobalPalette          = pPalette;
  pConfig->numGlobalPaletteEntries = numColors;
  pConfig->path                    = path;
  pConfig->attrFlags               = GIF_ATTR_IS_ANIMATED;   
}
static void initFrameConfig(FrameConfig* pConfig, uint8_t* pImageData, uint16_t delay) {
  memset(pConfig, 0, sizeof(FrameConfig));
  pConfig->delay      = delay;
  pConfig->pImageData = pImageData;
  pConfig->genFlags = FRAME_GEN_USE_TRANSPARENCY | FRAME_GEN_USE_DIFF_WINDOW; // use optimizations
}

/* This is an example code that creates a GIF-animation with a single moving pixel. */
int main(void) {
  GIF*        pGIF;                                                      // struct containing the GIF
  GIFConfig    gConfig;                                                  // global configuration parameters for the GIF
  FrameConfig  fConfig;                                                  // configuration parameters for a frame
  uint8_t*    pImageData;                                                // image data (an array of color-indices)
  int x,y;                                                               // position of the pixel
  uint8_t aPalette[] = {0xFF, 0x00, 0x00,                                // red
                        0x00, 0xFF, 0x00,                                // green
                        0x00, 0x00, 0xFF};                               // blue
  uint8_t numColors = 3;                                                 // number of colors in aPalette (up to 256 possible)
  int numFrames = 39*4;                                                  // number of frames in the video

  // initialize the GIF-configuration and create a new GIF
  initGIFConfig(&gConfig, "animated_single_pixel.gif", WIDTH, HEIGHT, aPalette, numColors);
  pGIF = cgif_newgif(&gConfig);

  // create image frames and add them to GIF
  pImageData = malloc(WIDTH * HEIGHT);                                   // allocate memory for image data
  memset(pImageData, 0, WIDTH * HEIGHT);                                 // set everything to the first color
  x = 0;                                                                 // start position of the pixel (x)
  y = 0;                                                                 // start position of the pixel (y) 
  for (int f = 0; f < numFrames; ++f) {                                  // loop over all frames
    pImageData[x + y*WIDTH] = 0;                                         // pixel at previous position back to the 1st color
    if (x==0 && y<WIDTH-1) {                                             // rules for moving the pixel
      y++;
    } else if (y==WIDTH-1 && x<WIDTH-1){
      x++;
    } else if (x==WIDTH-1 && y>0){
      y--;
    } else{
      x--;
    }
    pImageData[x + y*WIDTH] = 1;                                         // set color of next pixel to green
    initFrameConfig(&fConfig, pImageData, 5);                            // initialize the frame-configuration
    cgif_addframe(pGIF, &fConfig);                                       // append the new frame
  }
  free(pImageData);                                                      // free image data when all frames are added

  // close created GIF-file and free allocated space
  cgif_close(pGIF);
  return 0;
}
