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

static int read_gifconfig(ByteStream* pStream, CGIFrgb_Config* pDest) {
  int r;

  memset(pDest, 0, sizeof(CGIFrgb_Config));
  // width     : U16
  // height    : U16
  // attrFlags : U32
  // genFlags  : U32
  // numLoops  : U16
  r  = readdata(pStream, &pDest->width,                   2);
  r |= readdata(pStream, &pDest->height,                  2);
  r |= readdata(pStream, &pDest->attrFlags,               4);
  r |= readdata(pStream, &pDest->genFlags,                4);
  r |= readdata(pStream, &pDest->numLoops,                2);
  return (r ? 0 : 1);
}

static int read_frameconfig(ByteStream* pStream, CGIFrgb_FrameConfig* pDest, size_t sizeImageData) {
  int r;

  memset(pDest, 0, sizeof(CGIFrgb_FrameConfig));
  // attrFlags  : U16
  // genFlags   : U16
  // delay      : U16
  // fmtChan    : U8
  // pImageData : U8[sizeImageData]
  r  = readdata(pStream, &pDest->attrFlags,              2);
  r |= readdata(pStream, &pDest->genFlags,               2);
  r |= readdata(pStream, &pDest->delay,                  2);
  r |= readdata(pStream, &pDest->fmtChan,                1);
  if(pDest->fmtChan > 4) {
    return -1; // invalid fmtChan (avoid OOM)
  }
  pDest->pImageData = (uint8_t*)malloc(sizeImageData * pDest->fmtChan);
  if(pDest->pImageData == NULL) return 0; // malloc failed
  r |= readdata(pStream, pDest->pImageData, sizeImageData * pDest->fmtChan);
  if(r) {
    free(pDest->pImageData);
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
  CGIFrgb_Config      gconfig;
  CGIFrgb_FrameConfig fconfig;
  CGIFrgb*            pGIF;
  size_t              sizeImageData;
  int                 r;

  r = read_gifconfig(pStream, &gconfig);
  if(!r) {
    return -1;
  }
  sizeImageData = (uint32_t)gconfig.width * (uint32_t)gconfig.height;
  // limit dimensions of GIF to be created
  if(sizeImageData > (10000 * 10000)) {
    return -1;
  }
  gconfig.pWriteFn = writecb; // discard output
  pGIF             = cgif_rgb_newgif(&gconfig);
  if(pGIF == NULL) {
    return -1;
  }
  r = read_frameconfig(pStream, &fconfig, sizeImageData);
  while(r) {
    cgif_rgb_addframe(pGIF, &fconfig);
    free(fconfig.pImageData);
    r = read_frameconfig(pStream, &fconfig, sizeImageData);
  }
  r = cgif_rgb_close(pGIF);
  return r;
}

int LLVMFuzzerTestOneInput(const uint8_t* pData, size_t numBytes) {
  ByteStream input = { pData, numBytes, 0 };
  processInput(&input);
  return 0;
}
