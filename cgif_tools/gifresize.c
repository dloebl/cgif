#include <cgif.h>
#include <libnsgif.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion

/* callbacks for libnsgif */
static void* cb_create(int width, int height) {
  return malloc(width * height * 4);
}

static void cb_destroy(void* bitmap) {
  free(bitmap);
}

static uint8_t* cb_get_buffer(void* bitmap) {
  return (uint8_t*)bitmap;
}

/* helper function to get file size of open file in C99 */
static uint64_t getFileSize(FILE* pFile) {
  long curPos = ftell(pFile);
  fseek(pFile, 0, SEEK_END);
  long endPos = ftell(pFile);
  fseek(pFile, curPos, SEEK_SET);
  return endPos;
}

/* resize image without any color averaging and reusing (RGB) colors from before */
static uint8_t* resize_naive_rgb(const uint8_t* pImageData, uint16_t width, uint16_t height, uint16_t width_new, uint16_t height_new){
  uint8_t* pImgResized;
  uint16_t original_x, original_y;

  pImgResized = malloc(MULU16(3 * width_new, height_new)); // 3 for three colors in RGB
  for(uint16_t i = 0; i < height_new; ++i) {
    for(uint16_t j = 0; j < width_new; ++j) {
      original_y = MULU16(height, i) / height_new;
      original_x = MULU16(width, j) / width_new;
      pImgResized[MULU16(3 * i, width_new) + 3 * j]     = pImageData[MULU16(4 * original_y, width) + 4 * original_x];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 1] = pImageData[MULU16(4 * original_y, width) + 4 * original_x + 1];
      pImgResized[MULU16(3 * i, width_new) + 3 * j + 2] = pImageData[MULU16(4 * original_y, width) + 4 * original_x + 2];
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
  if(newWidth == 0 || newHeight == 0) {
    fprintf(stderr, "%s\n", "invalid new width or new height");
    return 2; // error    
  }  
  //
  // read in GIF via libnsgif
  FILE* pFile = fopen(sInput, "rb");
  if(!pFile) {
    fprintf(stderr, "%s\n", "failed to open <input-gif>");
    return 3; // error
  }
  uint64_t fileSize = getFileSize(pFile);
  uint8_t* pGIFData = malloc(fileSize);
  size_t cnt = fread(pGIFData, 1, fileSize, pFile);
  fclose(pFile);
  if(cnt != fileSize) {
    fprintf(stderr, "%s\n", "failed to read from <input-gif>");
    free(pGIFData);
    return 4; // error
  }

  // read GIF
  gif_animation dGIF;
  gif_bitmap_callback_vt bcb = { cb_create, cb_destroy, cb_get_buffer, NULL, NULL, NULL };
  gif_create(&dGIF, &bcb);
  gif_result r = gif_initialise(&dGIF, fileSize, pGIFData);
  //
  // check for errors
  if(r != GIF_OK) {
    fprintf(stderr, "%s\n", "<input-gif> is not a valid GIF");
    free(pGIFData);
    return 5;
  }

  uint8_t* pImageData = malloc(MULU16(newWidth, newHeight));
  uint8_t* aPalette = malloc(256 * 3); // max size of palette
  //
  // init GIF and Frame config
  CGIF_Config gConf      = {0};
  gConf.path             = sOutput;
  gConf.attrFlags        = CGIF_ATTR_IS_ANIMATED | CGIF_ATTR_NO_GLOBAL_TABLE;
  gConf.width            = newWidth;
  gConf.height           = newHeight;
  gConf.numLoops         = dGIF.loop_count;
  CGIF* eGIF             = cgif_newgif(&gConf);
  CGIF_FrameConfig fConf = {0};
  fConf.pLocalPalette    = aPalette;
  fConf.pImageData       = pImageData;
  fConf.attrFlags        = CGIF_FRAME_ATTR_USE_LOCAL_TABLE;
  fConf.genFlags         = CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW;
  //
  // encode frame by frame. generate one local color table per frame.
  for(uint32_t i = 0; i < dGIF.frame_count; ++i) {
    r = gif_decode_frame(&dGIF, i);
    if(r != GIF_OK) break; // error occurred
    fConf.delay = dGIF.frames[i].frame_delay;
    uint8_t* pRGBA   = dGIF.frame_image;
    uint8_t* pNewRGB = resize_naive_rgb(pRGBA, dGIF.width, dGIF.height, newWidth, newHeight);   // resize RGBA frame
    int cntBlock  = rgb_to_index(pNewRGB, MULU16(newWidth, newHeight), pImageData, aPalette); // get local palette from RGB frame
    fConf.numLocalPaletteEntries = cntBlock;
    cgif_addframe(eGIF, &fConf);
    free(pNewRGB);
  }
  cgif_close(eGIF);
  gif_finalise(&dGIF);
  //
  // free stuff
  free(pImageData);
  free(aPalette);  
  free(pGIFData);
  if(r != GIF_OK) {
    fprintf(stderr, "%s\n", "failed to decode frame");
    return 6;
  }
  return 0;
}
