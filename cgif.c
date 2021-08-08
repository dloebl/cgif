#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cgif.h"

#define HEADER_OFFSET_SIGNATURE    (0x00)
#define HEADER_OFFSET_VERSION      (0x03)
#define HEADER_OFFSET_WIDTH        (0x06)
#define HEADER_OFFSET_HEIGHT       (0x08)
#define HEADER_OFFSET_PACKED_FIELD (0x0A)
#define HEADER_OFFSET_BACKGROUND   (0x0B)
#define HEADER_OFFSET_MAP          (0x0C)

#define HEADER_WIDTH(a)            (*((uint16_t*)(a + HEADER_OFFSET_WIDTH )))
#define HEADER_HEIGHT(a)           (*((uint16_t*)(a + HEADER_OFFSET_HEIGHT)))

#define IMAGE_OFFSET_LEFT          (0x01)
#define IMAGE_OFFSET_TOP           (0x03)
#define IMAGE_OFFSET_WIDTH         (0x05)
#define IMAGE_OFFSET_HEIGHT        (0x07)
#define IMAGE_OFFSET_PACKED_FIELD  (0x09)

#define IMAGE_TOP(a)               (*((uint16_t*)(a + IMAGE_OFFSET_TOP)))
#define IMAGE_LEFT(a)              (*((uint16_t*)(a + IMAGE_OFFSET_LEFT)))
#define IMAGE_WIDTH(a)             (*((uint16_t*)(a + IMAGE_OFFSET_WIDTH )))
#define IMAGE_HEIGHT(a)            (*((uint16_t*)(a + IMAGE_OFFSET_HEIGHT)))
#define IMAGE_PACKED_FIELD(a)      (*((uint8_t*) (a + IMAGE_OFFSET_PACKED_FIELD)))

#define APPEXT_OFFSET_NAME            (0x03)
#define APPEXT_NETSCAPE_OFFSET_LOOPS  (APPEXT_OFFSET_NAME + 13)
#define NETSCAPE_LOOPS(a)             (*((uint16_t*)(a + APPEXT_NETSCAPE_OFFSET_LOOPS)))

#define GEXT_OFFSET_DELAY          (0x04)
#define GEXT_DELAY(a)              (*((uint16_t*)(a + GEXT_OFFSET_DELAY)))

#define MAX_CODE_LEN    12                    // maximum code length for lzw
#define MAX_DICT_LEN    (1uL << MAX_CODE_LEN) // maximum length of the dictionary
#define BLOCK_SIZE      0xFF                  // number of bytes in one block of the image data

typedef struct {
  uint16_t*       pTree;      // complete LZW dictionary tree in one block
  uint16_t*       pLZWData;   // pointer to LZW data
  const uint8_t*  pImageData; // pointer to image data
  uint32_t        numPixel;   // number of pixels per frame
  uint32_t        LZWPos;     // position of the current LZW code
  uint16_t        dictPos;    // currrent position in dictionary, we need to store 0-4096 -- so there are at least 13 bits needed here
} LZWGenState;

/* create new node in the tree that represents the dictionary of LZW-codes */
static void newNode(uint16_t* pTree, const uint16_t LZWIndex, const uint16_t initDictLen) {
  uint16_t* pNode;
  pNode = &(pTree[LZWIndex * initDictLen]);
  memset(pNode, 0, initDictLen * sizeof(uint16_t));
}

/* add new child node */
static void add_child(uint16_t* pTree, const uint16_t parentIndex, const uint16_t LZWIndex, const uint16_t initDictLen, const uint8_t index) {
  newNode(pTree, LZWIndex, initDictLen);
  pTree[parentIndex * initDictLen + index] = LZWIndex;
}

/* compute which initial LZW-code length is needed (shorter version?)*/
static uint8_t calcInitCodeLen(uint16_t numEntries) {
  if(numEntries > (1uL << 7)) {
    return 9;
  }
  if(numEntries > (1uL << 6)) {
    return 8; 
  }
  if(numEntries > (1uL << 5)) {
    return 7;
  }
  if(numEntries > (1uL << 4)) {
    return 6;
  }
  if(numEntries > (1uL << 3)) {
    return 5;
  }
  if(numEntries > (1uL << 2)) {
    return 4; 
  }
  return 3;
}

/* reset the dictionary of known LZW codes -- will reset the current code length as well */
static void resetDict(LZWGenState* pContext, const uint16_t initDictLen) {
  uint16_t i;

  pContext->dictPos                    = initDictLen + 2;                           // reset current position in dictionary (number of colors + 2 for start and end code)
  pContext->pLZWData[pContext->LZWPos] = initDictLen;                               // issue clear-code
  ++(pContext->LZWPos);                                                             // increment position in LZW data
  for(i = 0; i < initDictLen; ++i) {
    memset(&(pContext->pTree[i * initDictLen]), 0, initDictLen * sizeof(uint16_t)); // reset all egdes of root nodes of the LZW dictionary tree
  }
}

/* find next LZW code representing the longest pixel sequence that is still in the dictionary*/
static uint32_t lzw_crawl_tree(LZWGenState* pContext, uint32_t strPos, uint16_t parentIndex, const uint16_t initDictLen) {
  uint16_t nextPixel;

  while(strPos < (pContext->numPixel - 1)) {
    if((pContext->pTree[parentIndex * initDictLen + pContext->pImageData[strPos + 1]]) != 0) { // if pixel sequence is still in LZW-dictionary
      nextPixel   = pContext->pImageData[strPos + 1];                                          // get color index of next pixel in image data
      parentIndex = pContext->pTree[parentIndex * initDictLen + nextPixel];                    // the color index determines branch in the dictionary tree
      ++strPos;
    } else {                                                                                                       // if pixel sequence is not yet in the dictionary
      pContext->pLZWData[pContext->LZWPos] = parentIndex;                                                          // write last LZW code in LZW data
      ++(pContext->LZWPos);                                                                                        // increment position in LZW data
      if(pContext->dictPos < MAX_DICT_LEN) {                                                                       // if LZW-dictionary is not full yet
        add_child(pContext->pTree, parentIndex, pContext->dictPos, initDictLen, pContext->pImageData[strPos + 1]); // add new LZW code to dictionary
        ++(pContext->dictPos);                                                                                     // increase current position in the dictionary
      } else {
        // the dictionary reached its maximum code => reset it (not required by GIF-standard but mostly done like this)
        resetDict(pContext, initDictLen);
      }
      return strPos + 1;
    }
  }
  pContext->pLZWData[pContext->LZWPos] = parentIndex;                                     // if the end of the image is reached, write last LZW code
  ++(pContext->LZWPos);
  return strPos + 1;
}

/* generate LZW-codes that compress the image data*/
static uint32_t lzw_generate(Frame* pFrame, LZWGenState* pContext) {
  uint32_t strPos;
  uint8_t  parentIndex;

  strPos = 0;                                                                          // start at beginning of the image data
  resetDict(pContext, pFrame->initDictLen);                                            // reset dictionary and issue clear-code at first
  while(strPos < pContext->numPixel) {                                                 // while there are still image data to be encoded
    parentIndex  = pContext->pImageData[strPos];                                       // start at root node
    strPos       = lzw_crawl_tree(pContext, strPos, (uint16_t)parentIndex, pFrame->initDictLen); // get longest sequence that is still in dictionary, return new position in image data
  }
  pContext->pLZWData[pContext->LZWPos] = pFrame->initDictLen + 1;                      // termination code
  return pContext->LZWPos + 1;                                                         // return number of elements of LZW data
}

/* pack the LZW data into a byte sequence*/
static uint32_t create_byte_list(Frame* pFrame, uint8_t *byteList, uint32_t lzwPos, uint16_t *lzwStr){
  uint32_t i;
  uint32_t dictPos;                                                             // counting new LZW codes
  uint16_t n             = 2 * pFrame->initDictLen;                             // if n - pFrame->initDictLen == dictPos, the LZW code size is incremented by 1 bit
  uint32_t bytePos       = 0;                                                   // position of current byte
  uint8_t  bitOffset     = 0;                                                   // number of bits used in the last byte
  uint8_t  lzwCodeLen    = pFrame->initCodeLen;                                 // dynamically increasing length of the LZW codes
  int      correctLater  = 0;                                                   // 1: one empty byte too much if end is reached after current code, 0 otherwise

  byteList[0] = 0; // except from the 1st byte all other bytes should be initialized stepwise (below)
  // the very first symbol might be the clear-code. However, this is not mandatory. Quote:
  // "Encoders should output a Clear code as the first code of each image data stream."
  // We keep the option to NOT output the clear code as the first symbol in this function.
  dictPos     = 1;
  for(i = 0; i < lzwPos; ++i) {                                                 // loop over all LZW codes
    if((lzwCodeLen < MAX_CODE_LEN) && ((uint32_t)(n - (pFrame->initDictLen)) == dictPos)) { // larger code is used for the 1st time at i = 256 ...+ 512 ...+ 1024 -> 256, 768, 1792
      ++lzwCodeLen;                                                             // increment the length of the LZW codes (bit units)
      n *= 2;                                                                   // set threshold for next increment of LZW code size
    }
    correctLater       = 0;                                                     // 1 indicates that one empty byte is too much at the end
    byteList[bytePos] |= ((uint8_t)(lzwStr[i] << bitOffset));                   // add 1st bits of the new LZW code to the byte containing part of the previous code
    if(lzwCodeLen + bitOffset >= 8) {                                           // if the current byte is not enough of the LZW code
      if(lzwCodeLen + bitOffset == 8) {                                         // if just this byte is filled exactly
        byteList[++bytePos] = 0;                                                // byte is full -- go to next byte and initialize as 0
        correctLater        = 1;                                                // use if one 0byte to much at the end
      } else if(lzwCodeLen + bitOffset < 16) {                                  // if the next byte is not completely filled
        byteList[++bytePos] = (uint8_t)(lzwStr[i] >> (8-bitOffset));
      } else if(lzwCodeLen + bitOffset == 16) {                                 // if the next byte is exactly filled by LZW code
        byteList[++bytePos] = (uint8_t)(lzwStr[i] >> (8-bitOffset));
        byteList[++bytePos] = 0;                                                // byte is full -- go to next byte and initialize as 0
        correctLater        = 1;                                                // use if one 0byte to much at the end
      } else {                                                                  // lzw-code ranges over 3 bytes in total
        byteList[++bytePos] = (uint8_t)(lzwStr[i] >> (8-bitOffset));            // write part of LZW code to next byte
        byteList[++bytePos] = (uint8_t)(lzwStr[i] >> (16-bitOffset));           // write part of LZW code to byte after next byte
      }
    }
    bitOffset = (lzwCodeLen + bitOffset) % 8;                                   // how many bits of the last byte are used?
    ++dictPos;                                                                  // increment count of LZW codes
    if(lzwStr[i] == pFrame->initDictLen) {                                      // if a clear code appears in the LZW data
      lzwCodeLen = pFrame->initCodeLen;                                         // reset length of LZW codes
      n          = 2 * pFrame->initDictLen;                                     // reset threshold for next increment of LZW code length
      dictPos = 1;                                                              // reset (see comment below)
      // take first code already into account to increment lzwCodeLen exactly when the code length cannot represent the current maximum symbol.
      // Note: This is usually done implicitly, as the very first symbol is a clear-code itself.
    }
  }
  // comment: the last byte can be zero in the following case only:
  // terminate code has been written (initial dict length + 1), but current code size is larger so padding zero bits were added and extend into the next byte(s).
  if(correctLater) {                                                            // if an unneccessaray empty 0-byte was initialized at the end
    --bytePos;                                                                  // don't consider the last empty byte
  }
  return bytePos;
}

/* put byte sequence in blocks as required by GIF-format */
static uint32_t create_byte_list_block(uint8_t *byteList, uint8_t *byteListBlock, const uint32_t numBytes) {
  uint32_t i;
  uint32_t numBlock = numBytes / BLOCK_SIZE;                                                    // number of byte blocks with length BLOCK_SIZE
  uint8_t  numRest  = numBytes % BLOCK_SIZE;                                                    // number of bytes in last block (if not completely full)

  for(i = 0; i < numBlock; ++i) {                                                               // loop over all blocks
    byteListBlock[i * (BLOCK_SIZE+1)] = BLOCK_SIZE;                                             // number of bytes in the following block
    memcpy(byteListBlock + 1+i*(BLOCK_SIZE+1), byteList + i*BLOCK_SIZE, BLOCK_SIZE);            // copy block from byteList to byteListBlock
  }
  if(numRest>0) {
    byteListBlock[numBlock*(BLOCK_SIZE+1)] = numRest;                                           // number of bytes in the following block
    memcpy(byteListBlock + 1+numBlock*(BLOCK_SIZE+1), byteList + numBlock*BLOCK_SIZE, numRest); // copy block from byteList to byteListBlock
    byteListBlock[1 + numBlock * (BLOCK_SIZE + 1) + numRest] = 0;                               // set 0 at end of frame
    return 1 + numBlock * (BLOCK_SIZE + 1) + numRest;                                           // index of last entry in byteListBlock
  }
  // all LZW blocks in the frame have the same block size (BLOCK_SIZE), so there are no remaining bytes to be writen.
  byteListBlock[numBlock *(BLOCK_SIZE + 1)] = 0;                                                // set 0 at end of frame
  return numBlock *(BLOCK_SIZE + 1);                                                            // index of last entry in byteListBlock
}

/* create all LZW raster data in GIF-format */
static uint8_t* LZW_GenerateStream(Frame* pFrame, const uint32_t numPixel, const uint8_t* pImageData){
  LZWGenState* pContext;
  uint32_t     lzwPos, bytePos;
  uint32_t     bytePosBlock;

  pContext             = malloc(sizeof(LZWGenState)); // TBD check return value of malloc
  pContext->pTree      = malloc(sizeof(uint16_t) * pFrame->initDictLen * MAX_DICT_LEN); // TBD check return value of malloc
  pContext->numPixel   = numPixel;
  pContext->pImageData = pImageData;
  pContext->pLZWData   = malloc(sizeof(uint16_t) * (numPixel + 2)); // TBD check return value of malloc
  pContext->LZWPos     = 0;

  // actually generate the LZW sequence.
  lzwPos  = lzw_generate(pFrame, pContext);

  // pack the generated LZW data into blocks of 255 bytes
  uint8_t *byteList; // lzw-data packed in byte-list
  uint8_t *byteListBlock; // lzw-data packed in byte-list with 255-block structure
  uint64_t MaxByteListLen = MAX_CODE_LEN*lzwPos/8ul +2ul +1ul; // conservative upper bound
  uint64_t MaxByteListBlockLen = MAX_CODE_LEN*lzwPos*(BLOCK_SIZE+1ul)/8ul/BLOCK_SIZE +2ul +1ul +1ul; // conservative upper bound
  byteList      = malloc(MaxByteListLen); // TBD check return value of malloc
  byteListBlock = malloc(MaxByteListBlockLen); // TBD check return value of malloc
  bytePos = create_byte_list(pFrame, byteList,lzwPos, pContext->pLZWData);
  bytePosBlock = create_byte_list_block(byteList, byteListBlock, bytePos+1);
  free(byteList);
  free(pContext->pLZWData);
  free(pContext->pTree);
  free(pContext);
  pFrame->sizeRasterData = bytePosBlock + 1; // save 
  return byteListBlock;
}

/* initialize the header of the GIF */
static void initMainHeader(GIF* pGIF) {
  uint16_t width, height;
  uint8_t  x;
  uint8_t  initCodeLen;

  width           = pGIF->config.width;
  height          = pGIF->config.height;
  // calculate initial code length
  initCodeLen = calcInitCodeLen(pGIF->config.numGlobalPaletteEntries);

  // set header to a clean state
  memset(pGIF->aHeader, 0, sizeof(pGIF->aHeader));

  // set Signature field to value "GIF"
  pGIF->aHeader[HEADER_OFFSET_SIGNATURE]     = 'G';
  pGIF->aHeader[HEADER_OFFSET_SIGNATURE + 1] = 'I';
  pGIF->aHeader[HEADER_OFFSET_SIGNATURE + 2] = 'F';

  // set Version field to value "89a"
  pGIF->aHeader[HEADER_OFFSET_VERSION]       = '8';
  pGIF->aHeader[HEADER_OFFSET_VERSION + 1]   = '9'; 
  pGIF->aHeader[HEADER_OFFSET_VERSION + 2]   = 'a';

  // set width of screen
  HEADER_WIDTH(pGIF->aHeader)  = width; // TBD: works only on little endian system

  // set height of screen
  HEADER_HEIGHT(pGIF->aHeader) = height; // TBD: works only on little endian system

  // init packed field
  x = (pGIF->config.attrFlags & GIF_ATTR_NO_GLOBAL_TABLE) ? 0 : 1;
  pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] = (x << 7);                        // M = 1 (see GIF specs): Global color map is present
  if(x) {
    pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] |= ((initCodeLen - 2) << 0);     // set size of global color table
  }
  pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] |= (0uL << 4);                     // set color resolution (outdated - always zero)
}

/* initialize the global color table */
static void initGlobalColorTable(GIF* pGIF) {
  uint8_t*  pGlobalPalette;
  uint16_t  numGlobalPaletteEntries;

  pGlobalPalette          = pGIF->config.pGlobalPalette;
  numGlobalPaletteEntries = pGIF->config.numGlobalPaletteEntries;
  memset(pGIF->aGlobalColorTable, 0, sizeof(pGIF->aGlobalColorTable));
  memcpy(pGIF->aGlobalColorTable, pGlobalPalette, numGlobalPaletteEntries * 3);
}

/* initialize the local color table */
static void initLocalColorTable(Frame* pFrame) {
  uint8_t* pLocalPalette;
  uint16_t numLocalPaletteEntries;
  
  pLocalPalette          = pFrame->config.pLocalPalette;
  numLocalPaletteEntries = pFrame->config.numLocalPaletteEntries;
  memset(pFrame->aLocalColorTable, 0, sizeof(pFrame->aLocalColorTable));
  memcpy(pFrame->aLocalColorTable, pLocalPalette, numLocalPaletteEntries * 3);  
}

/* initialize NETSCAPE app extension block (needed for animation) */
static void initAppExtBlock(GIF* pGIF) {
  memset(pGIF->aAppExt, 0, sizeof(pGIF->aAppExt));

  // set data
  pGIF->aAppExt[0] = 0x21;
  pGIF->aAppExt[1] = 0xFF; // start of block
  pGIF->aAppExt[2] = 0x0B; // eleven bytes to follow
  
  // write identifier for Netscape animation extension
  pGIF->aAppExt[APPEXT_OFFSET_NAME]      = 'N';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 1]  = 'E';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 2]  = 'T';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 3]  = 'S';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 4]  = 'C';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 5]  = 'A';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 6]  = 'P';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 7]  = 'E';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 8]  = '2';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 9]  = '.';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 10] = '0';
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 11] = 0x03; // 3 bytes to follow
  pGIF->aAppExt[APPEXT_OFFSET_NAME + 12] = 0x01; // TBD clarify
  NETSCAPE_LOOPS(pGIF->aAppExt)          = pGIF->config.numLoops; // number of repetitions (animation), TBD: works only on little endian system
}

/* create a new GIF */
GIF* cgif_newgif(GIFConfig* pConfig) {
  GIF*     pGIF;
  uint8_t  initCodeLen;
  uint16_t initDictLen;

  pGIF = malloc(sizeof(GIF));
  if(pGIF == NULL) {
    return NULL; // error -> malloc failed
  }
  memcpy(&(pGIF->config), pConfig, sizeof(GIFConfig));

  // initiate all sections we can at this stage:
  // - main GIF header
  // - global color table, if required
  // - netscape application extension (for animation), if required
  initMainHeader(pGIF);

  // global color table required? => init it.
  if((pGIF->config.attrFlags & GIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    initGlobalColorTable(pGIF);  
  }
  // GIF should be animated? => init corresponding app extension header (NETSCAPE2.0)
  if(pConfig->attrFlags & GIF_ATTR_IS_ANIMATED) {
    initAppExtBlock(pGIF);
  }
  memset(&(pGIF->firstFrame), 0, sizeof(Frame));
  pGIF->pCurFrame = &(pGIF->firstFrame);
  
  // write first sections to file
  pGIF->pFile = fopen(pConfig->path, "wb"); // TBD check if fopen success
  fwrite(pGIF->aHeader, 1, 13, pGIF->pFile);
  if((pGIF->config.attrFlags & GIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    initCodeLen = calcInitCodeLen(pGIF->config.numGlobalPaletteEntries);
    initDictLen = 1uL << (initCodeLen - 1);
    fwrite(pGIF->aGlobalColorTable, 1, initDictLen * 3, pGIF->pFile);
  }
  if(pGIF->config.attrFlags & GIF_ATTR_IS_ANIMATED) {
    fwrite(pGIF->aAppExt, 1, 19, pGIF->pFile);
  }
  return pGIF;
}

/* optimize GIF file size by only redrawing the rectangular area that differs from previous frame */
static uint8_t* doWidthHeightOptim(uint8_t* pFrameHeader, uint8_t const* pCurImageData, uint8_t const* pBefImageData, const uint16_t width, const uint16_t height) {
  uint8_t* pNewImageData;
  uint16_t i, x, top;

  // find top 
  i = 0;
  while(i < (height - 1) && memcmp(pCurImageData + i * width, pBefImageData + i * width, width) == 0) {
  ++i;
  }
  top                     = i;
  IMAGE_TOP(pFrameHeader) = top;

  // find actual height
  i = height - 1;
  while(i > top && memcmp(pCurImageData + i * width, pBefImageData + i * width, width) == 0) {
    --i;
  }
  IMAGE_HEIGHT(pFrameHeader) = (i + 1) - top;

  // find left
  i = top;
  x = 0;
  while(pCurImageData[i * width + x] == pBefImageData[i * width + x]) {
    ++i;
    if(i > IMAGE_HEIGHT(pFrameHeader)) {
      if(x < width) {
        ++x;
        i = 0;
      } else {
        break;
      }
    }
  }
  IMAGE_LEFT(pFrameHeader) = x;

  // find actual width
  i = top;
  x = width - 1;
  while(pCurImageData[i * width + x] == pBefImageData[i * width + x]) {
    ++i;
    if(i > IMAGE_HEIGHT(pFrameHeader)) {
      if(x > IMAGE_LEFT(pFrameHeader)) {
        --x;
        i = 0;
      } else {
        break;
      }
    }
  }
  IMAGE_WIDTH(pFrameHeader) = (x + 1) - IMAGE_LEFT(pFrameHeader);

  // check whether we need a dummy pixel (frame is identical with one before)
  if (IMAGE_WIDTH(pFrameHeader) == 0 || IMAGE_HEIGHT(pFrameHeader) == 0) {
    IMAGE_WIDTH(pFrameHeader)  = 1;
    IMAGE_HEIGHT(pFrameHeader) = 1;
    IMAGE_LEFT(pFrameHeader)   = 0;
    IMAGE_TOP(pFrameHeader)    = 0;
  }

  // create new image data
  pNewImageData = malloc(IMAGE_WIDTH(pFrameHeader) * IMAGE_HEIGHT(pFrameHeader)); // TBD check return value of malloc
  for (i = 0; i < IMAGE_HEIGHT(pFrameHeader); ++i) {
    memcpy(pNewImageData + i * IMAGE_WIDTH(pFrameHeader), pCurImageData + (i + IMAGE_TOP(pFrameHeader)) * width + IMAGE_LEFT(pFrameHeader), IMAGE_WIDTH(pFrameHeader));
  }
  return pNewImageData;
}

/* add a new GIF-frame and write it */
int cgif_addframe(GIF* pGIF, FrameConfig* pConfig) {
  Frame*   pFrame;
  uint8_t* pTmpImageData;
  uint8_t* pBefImageData;
  uint16_t imageWidth;
  uint16_t imageHeight;
  uint16_t initialCodeSize; 
  uint32_t i, x;
  int      isFirstFrame;
  int      useLocalTable;
  int      hasTransparency;
  
  pFrame        = pGIF->pCurFrame;
  memcpy(&(pFrame->config), pConfig, sizeof(FrameConfig));
  imageWidth    = HEADER_WIDTH(pGIF->aHeader);
  imageHeight   = HEADER_HEIGHT(pGIF->aHeader);

  // determine fixed attributes of frame / GIF
  isFirstFrame    = ((&pGIF->firstFrame == pGIF->pCurFrame))                ? 1 : 0;
  useLocalTable   = (pFrame->config.attrFlags & FRAME_ATTR_USE_LOCAL_TABLE) ? 1 : 0;
  hasTransparency = (pGIF->config.attrFlags & GIF_ATTR_HAS_TRANSPARENCY)    ? 1 : 0;
  // deactivate impossible size optimizations 
  //  => in case the current frame or the frame before use a local-color table or transparency
  // FRAME_GEN_USE_TRANSPARENCY and FRAME_GEN_USE_DIFF_WINDOW are not possible
  if(isFirstFrame || useLocalTable || hasTransparency || (!isFirstFrame && (pFrame->pBef->config.attrFlags & FRAME_ATTR_USE_LOCAL_TABLE))) {
    pFrame->config.genFlags &= ~(FRAME_GEN_USE_TRANSPARENCY | FRAME_GEN_USE_DIFF_WINDOW);
  }

  // set Frame header to a clean state
  memset(pFrame->aImageHeader, 0, sizeof(pFrame->aImageHeader));

  // calculate initial code length and initial dict length
  if(useLocalTable) {
    pFrame->initCodeLen = calcInitCodeLen(pFrame->config.numLocalPaletteEntries);
  } else {
    pFrame->initCodeLen = calcInitCodeLen(pGIF->config.numGlobalPaletteEntries);
  }
  pFrame->initDictLen = 1uL << (pFrame->initCodeLen - 1);

  // set needed fields in frame header
  pFrame->aImageHeader[0] = ',';    // set frame seperator  
  if(useLocalTable) {
    initLocalColorTable(pFrame);
    IMAGE_PACKED_FIELD(pFrame->aImageHeader)  = (1 << 7);
    IMAGE_PACKED_FIELD(pFrame->aImageHeader) |= ((pFrame->initCodeLen - 2) << 0); // set size of local color table
  }

  // check if we need to increase the initial code length in order to allow the transparency optim.
  // note: In case the palette is full (256 entries) this optim is not possible
  if(pFrame->config.genFlags & FRAME_GEN_USE_TRANSPARENCY) {
    if (pFrame->initDictLen == pGIF->config.numGlobalPaletteEntries && pGIF->config.numGlobalPaletteEntries < 256) {
      ++(pFrame->initCodeLen);
      pFrame->initDictLen = 1uL << (pFrame->initCodeLen - 1);
    }
    pFrame->transIndex = pFrame->initDictLen - 1;
  }

  // copy given raw image data into the new FrameConfig, as we might need it in a later stage.
  pFrame->config.pImageData = malloc(imageWidth * imageHeight); // TBD check return value of malloc
  memcpy(pFrame->config.pImageData, pConfig->pImageData, imageWidth * imageHeight);

  // purge overlap of current frame and frame before (wdith - height optim), if required (FRAME_GEN_USE_DIFF_WINDOW set)
  if(pFrame->config.genFlags & FRAME_GEN_USE_DIFF_WINDOW) {
    pTmpImageData = doWidthHeightOptim(pFrame->aImageHeader, pConfig->pImageData, pFrame->pBef->config.pImageData, imageWidth, imageHeight);
  } else {
    IMAGE_WIDTH(pFrame->aImageHeader)  = imageWidth;
    IMAGE_HEIGHT(pFrame->aImageHeader) = imageHeight;
    pTmpImageData                      = NULL;
  }

  // mark matching areas of the previous frame as transparent, if required (FRAME_GEN_USE_TRANSPARENCY set)
  if((pFrame->config.genFlags & FRAME_GEN_USE_TRANSPARENCY) && pGIF->config.numGlobalPaletteEntries < 256) {
    if(pTmpImageData == NULL) {
      pTmpImageData = malloc(imageWidth * imageHeight); // TBD check return value of malloc
      memcpy(pTmpImageData, pConfig->pImageData, imageWidth * imageHeight);
    }
    pBefImageData = pFrame->pBef->config.pImageData;
    for(i = 0; i < IMAGE_HEIGHT(pFrame->aImageHeader); ++i) {
      for(x = 0; x < IMAGE_WIDTH(pFrame->aImageHeader); ++x) {
        if(pTmpImageData[i * IMAGE_WIDTH(pFrame->aImageHeader) + x] == pBefImageData[((IMAGE_TOP(pFrame->aImageHeader) + i) * imageWidth) + (IMAGE_LEFT(pFrame->aImageHeader) + x)]) {
          pTmpImageData[i * IMAGE_WIDTH(pFrame->aImageHeader) + x] = pFrame->transIndex;
        }
      }
    }
  }

  // generate LZW raster data (actual image data)
  if(((pFrame->config.genFlags & FRAME_GEN_USE_TRANSPARENCY) &&  pGIF->config.numGlobalPaletteEntries < 256) || (pFrame->config.genFlags & FRAME_GEN_USE_DIFF_WINDOW)) {
    pFrame->pRasterData = LZW_GenerateStream(pFrame, IMAGE_WIDTH(pFrame->aImageHeader) * IMAGE_HEIGHT(pFrame->aImageHeader), pTmpImageData);
    free(pTmpImageData);
  } else {
    pFrame->pRasterData = LZW_GenerateStream(pFrame, imageWidth * imageHeight, pConfig->pImageData);
  }

  // cleanup
  if(!isFirstFrame) {
    free(pFrame->pBef->config.pImageData);
  }
  pFrame->pNext             = malloc(sizeof(Frame)); // TBD check return value of malloc
  pFrame->pNext->transIndex = 0;
  pFrame->pNext->pBef       = pFrame;
  pFrame->pNext->pNext      = NULL;
  pGIF->pCurFrame           = pFrame->pNext;

  // do things for animation / user-defined transparency, if necessary
  if(pGIF->config.attrFlags & GIF_ATTR_IS_ANIMATED) {
    memset(pFrame->aGraphicExt, 0, sizeof(pFrame->aGraphicExt));
    pFrame->aGraphicExt[0] = 0x21;
    pFrame->aGraphicExt[1] = 0xF9;
    pFrame->aGraphicExt[2] = 0x04;
    if(hasTransparency) {
      pFrame->aGraphicExt[3] = 0x08; // restore original background
    } else {
      pFrame->aGraphicExt[3] = 0x04; // leave previous frame
    }
    if((pFrame->config.genFlags & FRAME_GEN_USE_TRANSPARENCY) || hasTransparency) {
      pFrame->aGraphicExt[3] |= 0x01;
    }
    pFrame->aGraphicExt[6] = pFrame->transIndex;
    GEXT_DELAY(pFrame->aGraphicExt) = pConfig->delay; // set delay (TBD: works only on little endian system)
  }

  // write frame to file
  initialCodeSize = pFrame->initCodeLen - 1;
  if(pGIF->config.attrFlags & GIF_ATTR_IS_ANIMATED) {
    fwrite(pFrame->aGraphicExt, 1, 8, pGIF->pFile);
  }
  fwrite(pFrame->aImageHeader, 1, 10, pGIF->pFile);
  if(pFrame->config.attrFlags & FRAME_ATTR_USE_LOCAL_TABLE) {
    fwrite(pFrame->aLocalColorTable, 1, pFrame->initDictLen * 3, pGIF->pFile);
  }
  fwrite(&initialCodeSize, 1, 1, pGIF->pFile);
  fwrite(pFrame->pRasterData, 1, pFrame->sizeRasterData, pGIF->pFile);  

  // free stuff
  free(pFrame->pRasterData);
  if(!isFirstFrame && (&(pGIF->firstFrame) != pFrame->pBef)) {
    free(pFrame->pBef);
  }
  return 0;
}

/* write terminate code, close the GIF-file, free allocated space */
int cgif_close(GIF* pGIF) {
  // not first frame?
  // => free preserved image data of the frame before
  fwrite(";", 1, 1, pGIF->pFile); // write term symbol to GIF-file 
  fclose(pGIF->pFile);            // we are done at this point => close the file
  if(&(pGIF->firstFrame) != pGIF->pCurFrame) {
    free(pGIF->pCurFrame->pBef->config.pImageData);
    if(pGIF->pCurFrame->pBef != &(pGIF->firstFrame)) {
      free(pGIF->pCurFrame->pBef);
    }
    free(pGIF->pCurFrame);
  }
  free(pGIF);
  return 0;
}
