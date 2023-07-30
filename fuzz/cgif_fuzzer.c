#include <cgif.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const uint8_t* pData;
  size_t         numBytes;
  size_t         curPos;
} ByteStream;

static int readdata(ByteStream* pStream, void* pDest, size_t size) {
  if((pStream->curPos + size) <= pStream->numBytes) {
    memcpy(pDest, &pStream->pData[pStream->curPos], size);
    pStream->curPos += size;
    return 0;
  }
  // error: out of bounds
  pStream->curPos = pStream->numBytes; // mark stream as out of sync
  return -1;
}

static int read_gifconfig(ByteStream* pStream, CGIF_Config* pDest) {
  int r;

  memset(pDest, 0, sizeof(CGIF_Config));
  // width     : U16
  // height    : U16
  // attrFlags : U32
  // genFlags  : U32
  // sizeGCT   : U16
  // numLoops  : U16
  // pGCT      : U8[sizeGCT * 3]
  r  = readdata(pStream, &pDest->width,                   2);
  r |= readdata(pStream, &pDest->height,                  2);
  r |= readdata(pStream, &pDest->attrFlags,               4);
  r |= readdata(pStream, &pDest->genFlags,                4);
  r |= readdata(pStream, &pDest->numGlobalPaletteEntries, 2);
  if(!(pDest->attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE)) {
    pDest->pGlobalPalette = (uint8_t*)malloc(pDest->numGlobalPaletteEntries * 3);
    if(pDest->pGlobalPalette == NULL) return 0; // malloc failed
    r |= readdata(pStream, pDest->pGlobalPalette, pDest->numGlobalPaletteEntries * 3);
  }
  if(r) {
    free(pDest->pGlobalPalette);
  }
  return (r ? 0 : 1);
}

static int read_frameconfig(ByteStream* pStream, CGIF_FrameConfig* pDest, size_t sizeImageData) {
  int r;

  memset(pDest, 0, sizeof(CGIF_FrameConfig));
  // attrFlags  : U16
  // genFlags   : U16
  // delay      : U16
  // transIndex : U8
  // sizeLCT    : U16
  // pLCT       : U8[sizeLCT * 3]
  // pImageData : U8[sizeImageData]
  r  = readdata(pStream, &pDest->attrFlags,              2);
  r |= readdata(pStream, &pDest->genFlags,               2);
  r |= readdata(pStream, &pDest->delay,                  2);
  r |= readdata(pStream, &pDest->transIndex,             1);
  r |= readdata(pStream, &pDest->numLocalPaletteEntries, 2);
  if(pDest->attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) {
    pDest->pLocalPalette = (uint8_t*)malloc(pDest->numLocalPaletteEntries * 3);
    if(pDest->pLocalPalette == NULL) return 0; // malloc failed
    r |= readdata(pStream, pDest->pLocalPalette, pDest->numLocalPaletteEntries * 3);
  }
  pDest->pImageData = (uint8_t*)malloc(sizeImageData);
  if(pDest->pImageData == NULL) return 0; // malloc failed
  r |= readdata(pStream, pDest->pImageData, sizeImageData);
  if(r) {
    free(pDest->pImageData);
    free(pDest->pLocalPalette);
  }
  return (r ? 0 : 1);
}

static int writecb(void* pContext, const uint8_t* pData, size_t size) {
  (void)pContext;
  (void)pData;
  (void)size;
  // eat input
  return 0;
}

static int processInput(ByteStream* pStream) {
  CGIF_Config      gconfig;
  CGIF_FrameConfig fconfig;
  CGIF*            pGIF;
  size_t           sizeImageData;
  int              r;

  r = read_gifconfig(pStream, &gconfig);
  if(!r) {
    return -1;
  }
  sizeImageData    = (uint32_t)gconfig.width * (uint32_t)gconfig.height;
  // limit dimensions of GIF to be created
  if(sizeImageData > (10000 * 10000)) {
    free(gconfig.pGlobalPalette);
    return -1;
  }
  gconfig.pWriteFn = writecb; // discard output
  pGIF             = cgif_newgif(&gconfig);
  free(gconfig.pGlobalPalette);
  if(pGIF == NULL) {
    return -1;
  }
  r = read_frameconfig(pStream, &fconfig, sizeImageData);
  while(r) {
    cgif_addframe(pGIF, &fconfig);
    free(fconfig.pImageData);
    free(fconfig.pLocalPalette);
    r = read_frameconfig(pStream, &fconfig, sizeImageData);
  }
  r = cgif_close(pGIF);
  return r;
}

#ifdef __cplusplus
extern "C"
#endif
int LLVMFuzzerTestOneInput(const uint8_t* pData, size_t numBytes) {
  ByteStream input = { pData, numBytes, 0 };
  processInput(&input);
  return 0;
}
