#include <cgif.h>
#include <gifdec.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion

/* resize image without any color averaging and reusing (RGB) colors from before */
static uint8_t* resize_naive_rgb(const uint8_t* pImageData, uint16_t width, uint16_t height, uint16_t width_new, uint16_t height_new){
  uint8_t* pImgResized;
  uint16_t original_x, original_y;

  pImgResized = malloc(MULU16(3 * width_new, height_new)); // 3 for three colors in RGB
  for(uint16_t i = 0; i < height_new; ++i) {
    for(uint16_t j = 0; j < width_new; ++j) {
      original_y = MULU16(height, i) / height_new;
      original_x = MULU16(width, j) / width_new;
      pImgResized[MULU16(3 * i, width_new) + 3 * j]     = pImageData[MULU16(3 * original_y, width) + 3 * original_x];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 1] = pImageData[MULU16(3 * original_y, width) + 3 * original_x + 1];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 2] = pImageData[MULU16(3 * original_y, width) + 3 * original_x + 2];
    }
  }
  return pImgResized;
}

/* convert rgb image to indices and palette 
   The overall number of colors must already be <= 256 which is e.g. the case when the image comes directly from a GIF */
// pImageDataRGB: image in RGB (input)
// numPixel:      number of pixels of input image
// pImageData:    pointer to image as indices (output)
// palette:       pointer to color palette (output)
static uint16_t rgb_to_index(const uint8_t* pImageDataRGB, uint32_t numPixel, uint8_t* pImageData, uint8_t* pPalette) {
  int       cnt       = 0;            // count the number of colors
  const int p         = 700001;       // prime number for hashing
  uint16_t  tableSize = 512;          // size of the hash table
  uint8_t   h;                        // hash modulo 256
  uint8_t   hashTable[3 * tableSize]; // stores RGB vales at position determined by hash
  uint8_t   colIdx[tableSize];        // index of the color
  uint8_t   indexUsed[tableSize];     // which part of the hash table was already used

  memset(indexUsed, 0, tableSize);
  for(uint32_t i = 0; i < numPixel; ++i){
    h = ((pImageDataRGB[3 * i + 0] * 256 * 256 + pImageDataRGB[3 * i + 1] * 256 + pImageDataRGB[3 * i + 2]) % p) % tableSize;
    while(1) {
      if(indexUsed[h] == 0){
        memcpy(&hashTable[3 * h], &pImageDataRGB[3 * i], 3); // add new color to hash table
        memcpy(&pPalette[3 * cnt], &pImageDataRGB[3 * i], 3); // add new color to palette
        indexUsed[h] = 1;
        colIdx[h] = cnt;
        pImageData[i] = cnt;
        ++cnt; // one new color added
        break;
      } else if(memcmp(&pImageDataRGB[3*i], &hashTable[3*h], 3) == 0){
        pImageData[i] = colIdx[h];
        break;
      } else {
        ++h; // go on searching for a free spot
      }
    }
    if(cnt > 256){
      fprintf(stderr, "%s\n", "too many colors");
      return cnt; // too many colors for indexing without quantization
    }
  }
  return cnt; // return number of colors found
}

// gifresize <input-gif> <output-gif> <new-width> <new-height>
int main(int argn, char** pArgs) {
  if(argn != 5) {
    fprintf(stderr, "%s\n", "invalid number of arguments\nsyntax:\ngifresize <input-gif> <output-gif> <new-width> <new-height>");
    return 1; // error
  }
  char* sInput = pArgs[1];
  char* sOutput = pArgs[2];
  uint16_t newWidth = atoi(pArgs[3]);
  uint16_t newHeight = atoi(pArgs[4]);
  if(newWidth == 0 && newHeight == 0) {
    fprintf(stderr, "%s\n", "invalid new width or new height");
    return 2; // error    
  }  
  //
  // read in GIF: done by libnsgif in case of libvips
  // TBD replace gifdec with libnsgif
  gd_GIF* dGIF  = gd_open_gif(sInput);  
  if(!dGIF) {
    fprintf(stderr, "%s\n", "failed to open <input-gif>");
    return 3;
  }
  uint8_t* pRGB = malloc(dGIF->width * dGIF->height * 3);
  uint8_t* pImageData = malloc(MULU16(newWidth, newHeight));
  uint8_t* aPalette = malloc(256 * 3); // max size of palette
  //
  // init GIF and Frame config
  CGIF_Config gConf      = {0};
  gConf.path             = sOutput;
  gConf.attrFlags        = CGIF_ATTR_IS_ANIMATED | CGIF_ATTR_NO_GLOBAL_TABLE;
  gConf.width            = newWidth;
  gConf.height           = newHeight;
  CGIF* eGIF             = cgif_newgif(&gConf);
  CGIF_FrameConfig fConf = {0};
  fConf.pLocalPalette    = aPalette;
  fConf.pImageData       = pImageData;
  fConf.attrFlags        = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConf.genFlags         = CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
  int r                  = gd_get_frame(dGIF);  
  //
  // encode frame by frame. generate one local color table per frame.
  while(r) {
    gd_render_frame(dGIF, pRGB); 
    fConf.delay = dGIF->gce.delay;
    uint8_t* pNewRGB = resize_naive_rgb(pRGB, dGIF->width, dGIF->height, newWidth, newHeight);   // resize RGB frame
    int cnt          = rgb_to_index(pNewRGB, MULU16(newWidth, newHeight), pImageData, aPalette); // get local palette from RGB frame
    fConf.numLocalPaletteEntries = cnt;
    cgif_addframe(eGIF, &fConf);
    r = gd_get_frame(dGIF);
    free(pNewRGB);
  }
  gd_close_gif(dGIF);
  cgif_close(eGIF);
  //
  // free stuff
  free(pRGB);
  free(pImageData);
  free(aPalette);  
  return 0;
}
