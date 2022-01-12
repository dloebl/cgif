#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cgif.h"
#include "cgif_raw.h"

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion
#define SIZE_FRAME_QUEUE (3)

// CGIF_Frame type
// note: internal sections, subject to change in future versions
typedef struct {
  CGIF_FrameConfig config;
  uint8_t          disposalMethod;
  uint8_t          transIndex;
} CGIF_Frame;

// CGIF type
// note: internal sections, subject to change in future versions
struct st_gif {
  CGIF_Frame*        aFrames[SIZE_FRAME_QUEUE]; // (internal) we need to keep the last three frames in memory.
  CGIF_Config        config;                    // (internal) configuration parameters of the GIF
  CGIFRaw*           pGIFRaw;                   // (internal) raw GIF stream
  FILE*              pFile;
  cgif_result        curResult;
};

// dimension result type
typedef struct {
  uint16_t width;
  uint16_t height;
  uint16_t top;
  uint16_t left;
} DimResult;

/* calculate next power of two exponent of given number (n MUST be <= 256) */
static uint8_t calcNextPower2Ex(uint16_t n) {
  uint8_t nextPow2;

  for (nextPow2 = 0; n > (1uL << nextPow2); ++nextPow2);
  return nextPow2;
}

/* write callback. returns 0 on success or -1 on error.  */
static int writecb(void* pContext, const uint8_t* pData, const size_t numBytes) {
  CGIF* pGIF;
  size_t r;

  pGIF = (CGIF*)pContext;
  if(pGIF->pFile) {
    r = fwrite(pData, 1, numBytes, pGIF->pFile);
    if(r == numBytes) return 0;
    else return -1;
  } else if(pGIF->config.pWriteFn) {
    return pGIF->config.pWriteFn(pGIF->config.pContext, pData, numBytes);
  }
  return 0;
}

/* create a new GIF */
CGIF* cgif_newgif(CGIF_Config* pConfig) {
  FILE*          pFile;
  CGIF*          pGIF;
  CGIFRaw*       pGIFRaw; // raw GIF stream
  CGIFRaw_Config rawConfig = {0};

  pFile = NULL;
  // open output file (if necessary)
  if(pConfig->path) {
    pFile = fopen(pConfig->path, "wb");
    if(pFile == NULL) {
      return NULL; // error: fopen failed
    }
  }
  // allocate space for CGIF context
  pGIF = malloc(sizeof(CGIF));
  if(pGIF == NULL) {
    if(pFile) {
      fclose(pFile);
    }
    return NULL; // error -> malloc failed
  }

  memset(pGIF, 0, sizeof(CGIF));
  pGIF->pFile = pFile;
  memcpy(&(pGIF->config), pConfig, sizeof(CGIF_Config));
  // make a deep copy of GCT, if required.
  if((pConfig->attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    pGIF->config.pGlobalPalette = malloc(pConfig->numGlobalPaletteEntries * 3);
    memcpy(pGIF->config.pGlobalPalette, pConfig->pGlobalPalette, pConfig->numGlobalPaletteEntries * 3);
  }

  rawConfig.pGCT      = pConfig->pGlobalPalette;
  rawConfig.sizeGCT   = (pConfig->attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) ? 0 : pConfig->numGlobalPaletteEntries;
  // translate CGIF_ATTR_* to CGIF_RAW_ATTR_* flags
  rawConfig.attrFlags = (pConfig->attrFlags & CGIF_ATTR_IS_ANIMATED) ? CGIF_RAW_ATTR_IS_ANIMATED : 0;
  rawConfig.width     = pConfig->width;
  rawConfig.height    = pConfig->height;
  rawConfig.numLoops  = pConfig->numLoops;
  rawConfig.pWriteFn  = writecb;
  rawConfig.pContext  = (void*)pGIF;
  // pass config down and create a new raw GIF stream.
  pGIFRaw = cgif_raw_newgif(&rawConfig);
  // check for errors
  if(pGIFRaw == NULL) {
    if(pFile) {
      fclose(pFile);
    }
    free(pGIF);
    return NULL;
  }

  pGIF->pGIFRaw = pGIFRaw;
  // assume error per default.
  // set to CGIF_OK by the first successful cgif_addframe() call, as a GIF without frames is invalid.
  pGIF->curResult = CGIF_PENDING;
  return pGIF;
}

/* compare given pixel indices using the correct local or global color table; returns 0 if the two pixels are RGB equal */
static int cmpPixel(const CGIF* pGIF, const CGIF_FrameConfig* pCur, const CGIF_FrameConfig* pBef, const uint8_t iCur, const uint8_t iBef) {
  uint8_t* pBefCT; // color table to use for pBef
  uint8_t* pCurCT; // color table to use for pCur

  // TBD add safety checks
  pBefCT = (pBef->attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? pBef->pLocalPalette : pGIF->config.pGlobalPalette; // local or global table used?
  pCurCT = (pCur->attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? pCur->pLocalPalette : pGIF->config.pGlobalPalette; // local or global table used?
  return memcmp(pBefCT + iBef * 3, pCurCT + iCur * 3, 3);
}

/* optimize GIF file size by only redrawing the rectangular area that differs from previous frame */
static uint8_t* doWidthHeightOptim(CGIF* pGIF, CGIF_FrameConfig* pCur, CGIF_FrameConfig* pBef, DimResult* pResult) {
  uint8_t* pNewImageData;
  const uint8_t* pCurImageData;
  const uint8_t* pBefImageData;
  uint16_t       i, x;
  uint16_t       newHeight, newWidth, newLeft, newTop;
  const uint16_t width  = pGIF->config.width;
  const uint16_t height = pGIF->config.height;
  uint8_t        iCur, iBef;

  pCurImageData = pCur->pImageData;
  pBefImageData = pBef->pImageData;
  // find top 
  i = 0;
  while(i < height) {
    for(int c = 0; c < width; ++c) {
      iCur = *(pCurImageData + MULU16(i, width) + c);
      iBef = *(pBefImageData + MULU16(i, width) + c);
      if(cmpPixel(pGIF, pCur, pBef, iCur, iBef) != 0) {
        goto FoundTop;
      }
    }
    ++i;
  }
FoundTop:
  if(i == height) { // need dummy pixel (frame is identical with one before)
    // TBD we might make it possible to merge identical frames in the future
    newWidth  = 1;
    newHeight = 1;
    newLeft   = 0;
    newTop    = 0;
    goto Done;
  }
  newTop = i;
  
  // find actual height
  i = height - 1;
  while(i > newTop) {
    for(int c = 0; c < width; ++c) {
      iCur = *(pCurImageData + MULU16(i, width) + c);
      iBef = *(pBefImageData + MULU16(i, width) + c);
      if(cmpPixel(pGIF, pCur, pBef, iCur, iBef) != 0) {
        goto FoundHeight;
      }
    }
    --i;
  }
FoundHeight:
  newHeight = (i + 1) - newTop;

  // find left
  i = newTop;
  x = 0;
  while(cmpPixel(pGIF, pCur, pBef, pCurImageData[MULU16(i, width) + x], pBefImageData[MULU16(i, width) + x]) == 0) {
    ++i;
    if(i > (newTop + newHeight - 1)) {
      ++x; //(x==width cannot happen as goto Done is triggered in the only possible case before)
      i = newTop;
    }
  }
  newLeft = x;

  // find actual width
  i = newTop;
  x = width - 1;
  while(cmpPixel(pGIF, pCur, pBef, pCurImageData[MULU16(i, width) + x], pBefImageData[MULU16(i, width) + x]) == 0) {
    ++i;
    if(i > (newTop + newHeight - 1)) {
      --x; //(x<newLeft cannot happen as goto Done is triggered in the only possible case before)
      i = newTop;
    }
  }
  newWidth = (x + 1) - newLeft;

Done:

  // create new image data
  pNewImageData = malloc(MULU16(newWidth, newHeight)); // TBD check return value of malloc
  for (i = 0; i < newHeight; ++i) {
    memcpy(pNewImageData + MULU16(i, newWidth), pCurImageData + MULU16((i + newTop), width) + newLeft, newWidth);
  }
  
  // set new width, height, top, left in DimResult struct
  pResult->width  = newWidth;
  pResult->height = newHeight;
  pResult->top    = newTop;
  pResult->left   = newLeft;
  return pNewImageData;
}

/* move frame down to the raw GIF API */
static cgif_result flushFrame(CGIF* pGIF, CGIF_Frame* pCur, CGIF_Frame* pBef) {
  CGIFRaw_FrameConfig rawConfig;
  DimResult           dimResult;
  uint8_t*            pTmpImageData;
  uint8_t*            pBefImageData;
  int                 isFirstFrame, useLCT, hasTrans;
  uint16_t            numPaletteEntries;
  uint16_t            imageWidth, imageHeight, width, height, top, left;
  uint8_t             transIndex, disposalMethod;
  cgif_result         r;

  imageWidth     = pGIF->config.width;
  imageHeight    = pGIF->config.height;
  isFirstFrame   = (pBef == NULL) ? 1 : 0;
  useLCT         = (pCur->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? 1 : 0;
  hasTrans       = (pGIF->config.attrFlags & CGIF_ATTR_HAS_TRANSPARENCY)      ? 1 : 0;
  disposalMethod = pCur->disposalMethod;
  transIndex     = pCur->transIndex;
  // deactivate impossible size optimizations 
  //  => in case user-defined transparency is used
  // CGIF_FRAME_GEN_USE_TRANSPARENCY and CGIF_FRAME_GEN_USE_DIFF_WINDOW are not possible
  if(isFirstFrame || hasTrans) {
    pCur->config.genFlags &= ~(CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW);
  }
  numPaletteEntries = (useLCT) ? pCur->config.numLocalPaletteEntries : pGIF->config.numGlobalPaletteEntries;
  // switch off transparency optimization if color table is full (no free spot for the transparent index), TBD: count used colors, adapt table
  if(numPaletteEntries == 256){
    pCur->config.genFlags &= ~CGIF_FRAME_GEN_USE_TRANSPARENCY;
  }

  // purge overlap of current frame and frame before (width - height optim), if required (CGIF_FRAME_GEN_USE_DIFF_WINDOW set)
  if(pCur->config.genFlags & CGIF_FRAME_GEN_USE_DIFF_WINDOW) {
    pTmpImageData = doWidthHeightOptim(pGIF, &pCur->config, &pBef->config, &dimResult);
    width  = dimResult.width;
    height = dimResult.height;
    top    = dimResult.top;
    left   = dimResult.left;
  } else {
    pTmpImageData = NULL;
    width         = imageWidth;
    height        = imageHeight;
    top           = 0;
    left          = 0;
  }

  // mark matching areas of the previous frame as transparent, if required (CGIF_FRAME_GEN_USE_TRANSPARENCY set)
  if(pCur->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY) {
    // set transIndex to next free index
    int pow2 = calcNextPower2Ex(numPaletteEntries);
    pow2 = (pow2 < 2) ? 2 : pow2; // TBD keep transparency index behavior as in V0.1.0 (for now)
    transIndex = (1 << pow2) - 1;
    if(transIndex < numPaletteEntries) {
      transIndex = (1 << (pow2 + 1)) - 1;
    }
    if(pTmpImageData == NULL) {
      pTmpImageData = malloc(MULU16(imageWidth, imageHeight)); // TBD check return value of malloc
      memcpy(pTmpImageData, pCur->config.pImageData, MULU16(imageWidth, imageHeight));
    }
    pBefImageData = pBef->config.pImageData;
    for(int i = 0; i < height; ++i) {
      for(int x = 0; x < width; ++x) {
        if(cmpPixel(pGIF, &pCur->config, &pBef->config, pTmpImageData[MULU16(i, width) + x], pBefImageData[MULU16(top + i, imageWidth) + (left + x)]) == 0) {
          pTmpImageData[MULU16(i, width) + x] = transIndex;
        }
      }
    }
  }

  // move frame down to GIF raw API
  rawConfig.pLCT           = pCur->config.pLocalPalette;
  rawConfig.pImageData     = (pTmpImageData) ? pTmpImageData : pCur->config.pImageData;
  rawConfig.attrFlags      = 0;
  if(hasTrans || (pCur->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY)) {
    rawConfig.attrFlags |= CGIF_RAW_FRAME_ATTR_HAS_TRANS;
  }
  rawConfig.width          = width;
  rawConfig.height         = height;
  rawConfig.top            = top;
  rawConfig.left           = left;
  rawConfig.delay          = pCur->config.delay;
  rawConfig.sizeLCT        = (useLCT) ? pCur->config.numLocalPaletteEntries : 0;
  rawConfig.disposalMethod = disposalMethod;
  rawConfig.transIndex     = transIndex;
  r = cgif_raw_addframe(pGIF->pGIFRaw, &rawConfig);
  free(pTmpImageData);
  return r;
}

static void freeFrame(CGIF_Frame* pFrame) {
  if(pFrame) {
    free(pFrame->config.pImageData);
    if(pFrame->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) {
      free(pFrame->config.pLocalPalette);
    }
    free(pFrame);
  }
}

/* queue a new GIF frame */
int cgif_addframe(CGIF* pGIF, CGIF_FrameConfig* pConfig) {
  CGIF_Frame* pNewFrame;
  int         i;
  cgif_result r;

  // check for previous errors
  if(pGIF->curResult != CGIF_OK && pGIF->curResult != CGIF_PENDING) {
    return pGIF->curResult;
  }
  // search for free slot in frame queue
  for(i = 1; i < SIZE_FRAME_QUEUE && pGIF->aFrames[i] != NULL; ++i);
  // check whether the queue is full
  // when queue is full: we need to flush one frame.
  if(i == SIZE_FRAME_QUEUE) {
    r = flushFrame(pGIF, pGIF->aFrames[1], pGIF->aFrames[0]);
    freeFrame(pGIF->aFrames[0]);
    // check for errors
    if(r != CGIF_OK) {
      pGIF->curResult = r;
      return pGIF->curResult;
    }
    i = SIZE_FRAME_QUEUE - 1;
    // keep the flushed frame in memory, as we might need it to write the next one.
    pGIF->aFrames[0] = pGIF->aFrames[1];
    pGIF->aFrames[1] = pGIF->aFrames[2];
  }
  // create new Frame struct + make a deep copy of pConfig.
  pNewFrame = malloc(sizeof(CGIF_Frame));
  memcpy(&(pNewFrame->config), pConfig, sizeof(CGIF_FrameConfig));
  pNewFrame->config.pImageData     = malloc(MULU16(pGIF->config.width, pGIF->config.height));
  memcpy(pNewFrame->config.pImageData, pConfig->pImageData, MULU16(pGIF->config.width, pGIF->config.height));
  // make a deep copy of the local color table, if required.
  if(pConfig->attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) {
    pNewFrame->config.pLocalPalette  = malloc(pConfig->numLocalPaletteEntries * 3);
    memcpy(pNewFrame->config.pLocalPalette, pConfig->pLocalPalette, pConfig->numLocalPaletteEntries * 3);
  }
  pNewFrame->disposalMethod        = DISPOSAL_METHOD_LEAVE;
  pNewFrame->transIndex            = 0;
  pGIF->aFrames[i]                 = pNewFrame; // add frame to queue
  // check whether we need to adapt the disposal method of the frame before.
  if(pGIF->config.attrFlags & CGIF_ATTR_HAS_TRANSPARENCY) {
    pGIF->aFrames[i]->disposalMethod = DISPOSAL_METHOD_BACKGROUND; // TBD might be removed
    pGIF->aFrames[i]->transIndex     = 0;
    if(pGIF->aFrames[i - 1] != NULL) {
      pGIF->aFrames[i - 1]->config.genFlags &= ~(CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW);
      pGIF->aFrames[i - 1]->disposalMethod   = DISPOSAL_METHOD_BACKGROUND; // restore to background color
    }
  }
  pGIF->curResult = CGIF_OK;
  return pGIF->curResult;
}

/* close the GIF-file and free allocated space */
int cgif_close(CGIF* pGIF) {
  int         r;
  cgif_result result;

  // check for previous errors
  if(pGIF->curResult != CGIF_OK) {
    goto CGIF_CLOSE_Cleanup;
  }

  // flush all remaining frames in queue
  for(int i = 1; i < SIZE_FRAME_QUEUE; ++i) {
    if(pGIF->aFrames[i] != NULL) {
      r = flushFrame(pGIF, pGIF->aFrames[i], pGIF->aFrames[i - 1]);
      if(r != CGIF_OK) {
        pGIF->curResult = r;
        break;
      }
    }
  }
  r = cgif_raw_close(pGIF->pGIFRaw); // close raw GIF stream
  // check for errors
  if(r != CGIF_OK) {
    pGIF->curResult = r;
  }

  // cleanup
CGIF_CLOSE_Cleanup:
  if(pGIF->pFile) {
    r = fclose(pGIF->pFile); // we are done at this point => close the file
    if(r) {
      pGIF->curResult = CGIF_ECLOSE; // error: fclose failed
    }
  }
  for(int i = 0; i < 3; ++i) {
    freeFrame(pGIF->aFrames[i]);
  }

  result = pGIF->curResult;
  if((pGIF->config.attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    free(pGIF->config.pGlobalPalette);
  }
  free(pGIF);
  // catch internal value CGIF_PENDING
  if(result == CGIF_PENDING) {
    result = CGIF_ERROR;
  }
  return result; // return previous result
}
