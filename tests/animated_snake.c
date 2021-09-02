#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

/* This is an example code that creates a GIF-animation with a moving snake. */
int main(void) {
  CGIF*        pGIF;                                                      // struct containing the GIF
  CGIF_Config    gConfig;                                                  // global configuration parameters for the GIF
  CGIF_FrameConfig  fConfig;                                                  // configuration parameters for a frame
  uint8_t*    pImageData;                                                // image data (an array of color-indices)
  int x,y,x1,y1;                                                         // position of snake beginning and end
  uint8_t aPalette[] = {0xFF, 0x00, 0x00,                                // red
                        0x00, 0xFF, 0x00,                                // green
                        0x00, 0x00, 0xFF};                               // blue
  uint8_t numColors = 3;                                                 // number of colors in aPalette (up to 256 possible)
  int numFrames = 37*4;                                                  // number of frames in the video

  // initialize the GIF-configuration and create a new GIF
  initGIFConfig(&gConfig, "animated_snake.gif", WIDTH, HEIGHT, aPalette, numColors);
  pGIF = cgif_newgif(&gConfig);

  // create image frames and add them to GIF
  pImageData = malloc(WIDTH * HEIGHT);                                   // allocate memory for image data
  memset(pImageData, 0, WIDTH * HEIGHT);                                 // set the background color to 1st color in palette
  x = 1;                                                                 // set start position...
  y = 1;
  x1 = 1;
  y1 = 15;
  for (int i = 1; i < 15; ++i) {                                         // draw the snake
    pImageData[1 + i*WIDTH] = 1;
  }
  for (int f = 0; f < numFrames; ++f) {                                  // loop over all frames
    pImageData[x + y*WIDTH] = 0;                                         // remove end of the snake
    pImageData[x1 + y1*WIDTH] = 1;                                       // moving pixel at snake head
    if (x==1 && y<WIDTH-2) {                                             // rules for moving the snake...
      y++;
    } else if (y==WIDTH-2 && x<WIDTH-2){
      x++;
    } else if (x==WIDTH-2 && y>1){
      y--;
    } else{
      x--;
    }
    if (x1==1 && y1<WIDTH-2) {
      y1++;
    } else if (y1==WIDTH-2 && x1<WIDTH-2){
      x1++;
    } else if (x1==WIDTH-2 && y1>1){
      y1--;
    } else{
      x1--;
    }
    initFrameConfig(&fConfig, pImageData, 5);                            // initialize the frame-configuration
    cgif_addframe(pGIF, &fConfig);                                       // append the new frame
  }
  free(pImageData);                                                      // free image data when all frames are added

  // close created GIF-file and free allocated space
  cgif_close(pGIF);
  return 0;
}
