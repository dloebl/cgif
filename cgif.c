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

#define IMAGE_OFFSET_LEFT          (0x01)
#define IMAGE_OFFSET_TOP           (0x03)
#define IMAGE_OFFSET_WIDTH         (0x05)
#define IMAGE_OFFSET_HEIGHT        (0x07)
#define IMAGE_OFFSET_PACKED_FIELD  (0x09)

#define IMAGE_PACKED_FIELD(a)      (*((uint8_t*) (a + IMAGE_OFFSET_PACKED_FIELD)))

#define APPEXT_OFFSET_NAME            (0x03)
#define APPEXT_NETSCAPE_OFFSET_LOOPS  (APPEXT_OFFSET_NAME + 13)

#define GEXT_OFFSET_DELAY          (0x04)

#define MAX_CODE_LEN    12                    // maximum code length for lzw
#define MAX_DICT_LEN    (1uL << MAX_CODE_LEN) // maximum length of the dictionary
#define BLOCK_SIZE      0xFF                  // number of bytes in one block of the image data

#define MULU16(a, b) (((uint32_t)a) * ((uint32_t)b)) // helper macro to correctly multiply two U16's without default signed int promotion

typedef struct {
  uint16_t*       pTree;      // complete LZW dictionary tree in one block
  uint16_t*       pLZWData;   // pointer to LZW data
  const uint8_t*  pImageData; // pointer to image data
  uint32_t        numPixel;   // number of pixels per frame
  uint32_t        LZWPos;     // position of the current LZW code
  uint16_t        dictPos;    // currrent position in dictionary, we need to store 0-4096 -- so there are at least 13 bits needed here
} LZWGenState;

/* converts host U16 to little-endian (LE) U16 */
static uint16_t hU16toLE(const uint16_t n) {
  int      isBE;
  uint16_t newVal;
  uint16_t one;

  one    = 1;
  isBE   = *((uint8_t*)&one) ? 0 : 1;
  if(isBE) {
    newVal = (n >> 8) | (n << 8);
  } else {
    newVal = n; // already LE
  }
  return newVal;
}

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

/* calculate next power of two exponent of given number (n MUST be <= 256) */
static uint8_t calcNextPower2Ex(uint16_t n) {
  uint8_t nextPow2;

  for (nextPow2 = 0; n > (1uL << nextPow2); ++nextPow2);
  return nextPow2;
}

/* compute which initial LZW-code length is needed */
static uint8_t calcInitCodeLen(uint16_t numEntries) {
  uint8_t index;

  index = calcNextPower2Ex(numEntries);
  return (index < 3) ? 3 : index + 1;
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
static uint32_t lzw_generate(CGIF_Frame* pFrame, LZWGenState* pContext) {
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
static uint32_t create_byte_list(CGIF_Frame* pFrame, uint8_t *byteList, uint32_t lzwPos, uint16_t *lzwStr){
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
static uint8_t* LZW_GenerateStream(CGIF_Frame* pFrame, const uint32_t numPixel, const uint8_t* pImageData){
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
static void initMainHeader(CGIF* pGIF) {
  uint16_t width, height;
  uint8_t  x;
  uint8_t  pow2GlobalPalette;

  width           = pGIF->config.width;
  height          = pGIF->config.height;

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

  // set width of screen (LE ordering)
  const uint16_t widthLE  = hU16toLE(width);
  memcpy(pGIF->aHeader + HEADER_OFFSET_WIDTH, &widthLE, sizeof(uint16_t));

  // set height of screen (LE ordering)
  const uint16_t heightLE = hU16toLE(height);
  memcpy(pGIF->aHeader + HEADER_OFFSET_HEIGHT, &heightLE, sizeof(uint16_t));

  // init packed field
  x = (pGIF->config.attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) ? 0 : 1;
  pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] = (x << 7);                        // M = 1 (see GIF specs): Global color map is present
  if(x) {
    // calculate needed size of global color table
    pow2GlobalPalette = calcNextPower2Ex(pGIF->config.numGlobalPaletteEntries);
    pow2GlobalPalette = (pow2GlobalPalette < 1) ? 1 : pow2GlobalPalette;      // minimum size is 2^1
    pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] |= ((pow2GlobalPalette - 1) << 0);     // set size of global color table (0 - 7 in header + 1)
  }
  pGIF->aHeader[HEADER_OFFSET_PACKED_FIELD] |= (0uL << 4);                     // set color resolution (outdated - always zero)
}

/* initialize the global color table */
static void initGlobalColorTable(CGIF* pGIF) {
  uint8_t*  pGlobalPalette;
  uint16_t  numGlobalPaletteEntries;

  pGlobalPalette          = pGIF->config.pGlobalPalette;
  numGlobalPaletteEntries = pGIF->config.numGlobalPaletteEntries;
  memset(pGIF->aGlobalColorTable, 0, sizeof(pGIF->aGlobalColorTable));
  memcpy(pGIF->aGlobalColorTable, pGlobalPalette, numGlobalPaletteEntries * 3);
}

/* initialize the local color table */
static void initLocalColorTable(CGIF_Frame* pFrame) {
  uint8_t* pLocalPalette;
  uint16_t numLocalPaletteEntries;
  
  pLocalPalette          = pFrame->config.pLocalPalette;
  numLocalPaletteEntries = pFrame->config.numLocalPaletteEntries;
  memset(pFrame->aLocalColorTable, 0, sizeof(pFrame->aLocalColorTable));
  memcpy(pFrame->aLocalColorTable, pLocalPalette, numLocalPaletteEntries * 3);  
}

/* initialize NETSCAPE app extension block (needed for animation) */
static void initAppExtBlock(CGIF* pGIF) {
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
  // set number of repetitions (animation; LE ordering)
  const uint16_t netscapeLE = hU16toLE(pGIF->config.numLoops);
  memcpy(pGIF->aAppExt + APPEXT_NETSCAPE_OFFSET_LOOPS, &netscapeLE, sizeof(uint16_t));
}

/* write a chunk of data to either a callback or a file */
static int write(CGIF* pGIF, const uint8_t* pData, const size_t numBytes) {
  if(pGIF->pFile) {
    return fwrite(pData, 1, numBytes, pGIF->pFile);
  } else if(pGIF->config.pWriteFn) {
    return pGIF->config.pWriteFn(pGIF->config.pContext, pData, numBytes);
  }
  return 0;
}

/* create a new GIF */
CGIF* cgif_newgif(CGIF_Config* pConfig) {
  CGIF*     pGIF;
  uint8_t  pow2GlobalPalette;

  pGIF = malloc(sizeof(CGIF));
  if(pGIF == NULL) {
    return NULL; // error -> malloc failed
  }
  memcpy(&(pGIF->config), pConfig, sizeof(CGIF_Config));

  // initiate all sections we can at this stage:
  // - main GIF header
  // - global color table, if required
  // - netscape application extension (for animation), if required
  initMainHeader(pGIF);

  // global color table required? => init it.
  if((pGIF->config.attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    initGlobalColorTable(pGIF);  
  }
  // GIF should be animated? => init corresponding app extension header (NETSCAPE2.0)
  if(pConfig->attrFlags & CGIF_ATTR_IS_ANIMATED) {
    initAppExtBlock(pGIF);
  }
  memset(&(pGIF->firstFrame), 0, sizeof(CGIF_Frame));
  pGIF->pCurFrame = &(pGIF->firstFrame);
  
  // write first sections to file
  pGIF->pFile = NULL;
  if(pConfig->path) {
    pGIF->pFile = fopen(pConfig->path, "wb"); // TBD check if fopen success
  }
  write(pGIF, pGIF->aHeader, 13);
  if((pGIF->config.attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE) == 0) {
    pow2GlobalPalette = calcNextPower2Ex(pGIF->config.numGlobalPaletteEntries);
    pow2GlobalPalette = (pow2GlobalPalette < 1) ? 1 : pow2GlobalPalette; // minimum size is 2^1
    write(pGIF, pGIF->aGlobalColorTable, 3 << pow2GlobalPalette);
  }
  if(pGIF->config.attrFlags & CGIF_ATTR_IS_ANIMATED) {
    write(pGIF, pGIF->aAppExt, 19);
  }
  return pGIF;
}

/* compare given pixel indices using the correct local or global color table; returns 0 if the two pixels are RGB equal */
static int cmpPixel(CGIF* pGIF, CGIF_Frame* pCur, CGIF_Frame* pBef, const uint8_t iCur, const uint8_t iBef) {
  uint8_t* pBefCT; // color table to use for pBef
  uint8_t* pCurCT; // color table to use for pCur

  // TBD add safety checks
  pBefCT = (pBef->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? pBef->aLocalColorTable : pGIF->aGlobalColorTable; // local or global table used?
  pCurCT = (pCur->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? pCur->aLocalColorTable : pGIF->aGlobalColorTable; // local or global table used?
  return memcmp(pBefCT + iBef * 3, pCurCT + iCur * 3, 3);
}

/* optimize GIF file size by only redrawing the rectangular area that differs from previous frame */
static uint8_t* doWidthHeightOptim(CGIF* pGIF, CGIF_Frame* pFrame, uint8_t const* pCurImageData, uint8_t const* pBefImageData, const uint16_t width, const uint16_t height) {
  uint8_t* pNewImageData;
  uint16_t i, x;
  uint16_t newHeight, newWidth, newLeft, newTop;
  uint8_t iCur, iBef;

  // find top 
  i = 0;
  while(i < height) {
    for(int c = 0; c < width; ++c) {
      iCur = *(pCurImageData + MULU16(i, width) + c);
      iBef = *(pBefImageData + MULU16(i, width) + c);
      if(cmpPixel(pGIF, pFrame, pFrame->pBef, iCur, iBef) != 0) {
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
      if(cmpPixel(pGIF, pFrame, pFrame->pBef, iCur, iBef) != 0) {
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
  while(cmpPixel(pGIF, pFrame, pFrame->pBef, pCurImageData[MULU16(i, width) + x], pBefImageData[MULU16(i, width) + x]) == 0) {
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
  while(cmpPixel(pGIF, pFrame, pFrame->pBef, pCurImageData[MULU16(i, width) + x], pBefImageData[MULU16(i, width) + x]) == 0) {
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
  
  // set new width, height, top, left in Frame struct
  pFrame->width  = newWidth;
  pFrame->height = newHeight;
  pFrame->top    = newTop;
  pFrame->left   = newLeft;
  return pNewImageData;
}

/* add a new GIF-frame and write it */
int cgif_addframe(CGIF* pGIF, CGIF_FrameConfig* pConfig) {
  CGIF_Frame*   pFrame;
  uint8_t* pTmpImageData;
  uint8_t* pBefImageData;
  uint16_t imageWidth;
  uint16_t imageHeight;
  uint8_t  initialCodeSize;
  uint32_t i, x;
  int      isFirstFrame;
  int      useLocalTable;
  int      hasTransparency;
  uint8_t  pow2LocalPalette;
  
  pFrame        = pGIF->pCurFrame;
  memcpy(&(pFrame->config), pConfig, sizeof(CGIF_FrameConfig));
  imageWidth    = pGIF->config.width;
  imageHeight   = pGIF->config.height;

  // determine fixed attributes of frame / GIF
  isFirstFrame    = ((&pGIF->firstFrame == pGIF->pCurFrame))                ? 1 : 0;
  useLocalTable   = (pFrame->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) ? 1 : 0;
  hasTransparency = (pGIF->config.attrFlags & CGIF_ATTR_HAS_TRANSPARENCY)    ? 1 : 0;
  // deactivate impossible size optimizations 
  //  => in case user-defined transparency is used
  // CGIF_FRAME_GEN_USE_TRANSPARENCY and CGIF_FRAME_GEN_USE_DIFF_WINDOW are not possible
  if(isFirstFrame || hasTransparency) {
    pFrame->config.genFlags &= ~(CGIF_FRAME_GEN_USE_TRANSPARENCY | CGIF_FRAME_GEN_USE_DIFF_WINDOW);
  }
  // switch off transparency optimization if color table is full (no free spot for the transparent index), TBD: count used colors, adapt table
  if(pGIF->config.numGlobalPaletteEntries == 256){
    pFrame->config.genFlags &= ~CGIF_FRAME_GEN_USE_TRANSPARENCY;
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
    // calculate needed size of local color table
    pow2LocalPalette = calcNextPower2Ex(pFrame->config.numLocalPaletteEntries);
    pow2LocalPalette = (pow2LocalPalette < 1) ? 1 : pow2LocalPalette;            // minimum size is 2^1
    IMAGE_PACKED_FIELD(pFrame->aImageHeader)  = (1 << 7);
    IMAGE_PACKED_FIELD(pFrame->aImageHeader) |= ((pow2LocalPalette - 1) << 0); // set size of local color table (0 - 7 in header + 1)
  }

  // check if we need to increase the initial code length in order to allow the transparency optim.
  // TBD revert this in case no transparency is possible
  if(pFrame->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY) {
    if(pFrame->initDictLen == (useLocalTable ? pFrame->config.numLocalPaletteEntries : pGIF->config.numGlobalPaletteEntries)) {
      ++(pFrame->initCodeLen);
      pFrame->initDictLen = 1uL << (pFrame->initCodeLen - 1);
    }
    pFrame->transIndex = pFrame->initDictLen - 1;
  }

  // copy given raw image data into the new CGIF_FrameConfig, as we might need it in a later stage.
  pFrame->config.pImageData = malloc(MULU16(imageWidth, imageHeight)); // TBD check return value of malloc
  memcpy(pFrame->config.pImageData, pConfig->pImageData, MULU16(imageWidth, imageHeight));

  // purge overlap of current frame and frame before (wdith - height optim), if required (CGIF_FRAME_GEN_USE_DIFF_WINDOW set)
  if(pFrame->config.genFlags & CGIF_FRAME_GEN_USE_DIFF_WINDOW) {
    pTmpImageData = doWidthHeightOptim(pGIF, pFrame, pConfig->pImageData, pFrame->pBef->config.pImageData, imageWidth, imageHeight);
  } else {
    pFrame->width                      = imageWidth;
    pFrame->height                     = imageHeight;
    pFrame->top                        = 0;
    pFrame->left                       = 0;
    pTmpImageData                      = NULL;
  }
  const uint16_t frameWidthLE  = hU16toLE(pFrame->width);
  const uint16_t frameHeightLE = hU16toLE(pFrame->height);
  const uint16_t frameTopLE    = hU16toLE(pFrame->top);
  const uint16_t frameLeftLE   = hU16toLE(pFrame->left);
  memcpy(pFrame->aImageHeader + IMAGE_OFFSET_WIDTH,  &frameWidthLE,  sizeof(uint16_t));
  memcpy(pFrame->aImageHeader + IMAGE_OFFSET_HEIGHT, &frameHeightLE, sizeof(uint16_t));
  memcpy(pFrame->aImageHeader + IMAGE_OFFSET_TOP,    &frameTopLE,    sizeof(uint16_t));
  memcpy(pFrame->aImageHeader + IMAGE_OFFSET_LEFT,   &frameLeftLE,   sizeof(uint16_t));
  // mark matching areas of the previous frame as transparent, if required (CGIF_FRAME_GEN_USE_TRANSPARENCY set)
  if(pFrame->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY) {
    if(pTmpImageData == NULL) {
      pTmpImageData = malloc(MULU16(imageWidth, imageHeight)); // TBD check return value of malloc
      memcpy(pTmpImageData, pConfig->pImageData, MULU16(imageWidth, imageHeight));
    }
    pBefImageData = pFrame->pBef->config.pImageData;
    for(i = 0; i < pFrame->height; ++i) {
      for(x = 0; x < pFrame->width; ++x) {
        if(cmpPixel(pGIF, pFrame, pFrame->pBef, pTmpImageData[MULU16(i, pFrame->width) + x], pBefImageData[MULU16(pFrame->top + i, imageWidth) + (pFrame->left + x)]) == 0) {
          pTmpImageData[MULU16(i, pFrame->width) + x] = pFrame->transIndex;
        }
      }
    }
  }

  // generate LZW raster data (actual image data)
  if((pFrame->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY) || (pFrame->config.genFlags & CGIF_FRAME_GEN_USE_DIFF_WINDOW)) {
    pFrame->pRasterData = LZW_GenerateStream(pFrame, MULU16(pFrame->width, pFrame->height), pTmpImageData);
    free(pTmpImageData);
  } else {
    pFrame->pRasterData = LZW_GenerateStream(pFrame, MULU16(imageWidth, imageHeight), pConfig->pImageData);
  }

  // cleanup
  if(!isFirstFrame) {
    free(pFrame->pBef->config.pImageData);
  }
  pFrame->pNext             = malloc(sizeof(CGIF_Frame)); // TBD check return value of malloc
  pFrame->pNext->transIndex = 0;
  pFrame->pNext->pBef       = pFrame;
  pFrame->pNext->pNext      = NULL;
  pGIF->pCurFrame           = pFrame->pNext;

  // do things for animation / user-defined transparency, if necessary
  if(pGIF->config.attrFlags & CGIF_ATTR_IS_ANIMATED) {
    memset(pFrame->aGraphicExt, 0, sizeof(pFrame->aGraphicExt));
    pFrame->aGraphicExt[0] = 0x21;
    pFrame->aGraphicExt[1] = 0xF9;
    pFrame->aGraphicExt[2] = 0x04;
    if(hasTransparency) {
      pFrame->aGraphicExt[3] = 0x08; // restore original background
    } else {
      pFrame->aGraphicExt[3] = 0x04; // leave previous frame
    }
    if((pFrame->config.genFlags & CGIF_FRAME_GEN_USE_TRANSPARENCY) || hasTransparency) {
      pFrame->aGraphicExt[3] |= 0x01; // set flag indicating that transparency is used
    }
    pFrame->aGraphicExt[6] = pFrame->transIndex;
    // set delay (LE ordering)
    const uint16_t delayLE = hU16toLE(pConfig->delay);
    memcpy(pFrame->aGraphicExt + GEXT_OFFSET_DELAY, &delayLE, sizeof(uint16_t));
  }

  // write frame
  initialCodeSize = pFrame->initCodeLen - 1;
  if(pGIF->config.attrFlags & CGIF_ATTR_IS_ANIMATED) {
    write(pGIF, pFrame->aGraphicExt, 8);
  }
  write(pGIF, pFrame->aImageHeader, 10);
  if(pFrame->config.attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) {
    write(pGIF, pFrame->aLocalColorTable, 3 << pow2LocalPalette);
  }
  write(pGIF, &initialCodeSize, 1);
  write(pGIF, pFrame->pRasterData, pFrame->sizeRasterData);

  // free stuff
  free(pFrame->pRasterData);
  if(!isFirstFrame && (&(pGIF->firstFrame) != pFrame->pBef)) {
    free(pFrame->pBef);
  }
  return 0;
}

/* write terminate code, close the GIF-file, free allocated space */
int cgif_close(CGIF* pGIF) {
  // not first frame?
  // => free preserved image data of the frame before
  write(pGIF, (unsigned char*) ";", 1); // write term symbol
  if(pGIF->pFile) {
    fclose(pGIF->pFile);            // we are done at this point => close the file
  }
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
