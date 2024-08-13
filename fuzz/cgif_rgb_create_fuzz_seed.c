/*
  Create seed corpus from our test cases (tests/).
  Wrap cgif's rgb API and write configuration to CGIF_OUTPATH.
*/

#include <cgif.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef CGIF_OUTPATH
  #error "please set a output path: define CGIF_OUTPATH"
#endif

typedef struct st_cgif_rgb {
  CGIFrgb_Config config;
  FILE*       pFile;
  cgif_result curResult;
} CGIFrgb;

static int writedata(FILE* pFile, const void* pData, uint32_t size) {
  int r = fwrite(pData, size, 1, pFile);
  if(r != 1) return 1;
  return 0;
}

static int write_gifconfig(FILE* pFile, const CGIFrgb_Config* pConfig) {
  int r;
  // width     : U16
  // height    : U16
  // attrFlags : U32
  // genFlags  : U32
  // numLoops  : U16
  r  = writedata(pFile, &pConfig->width,                   2);
  r |= writedata(pFile, &pConfig->height,                  2);
  r |= writedata(pFile, &pConfig->attrFlags,               4);
  r |= writedata(pFile, &pConfig->genFlags,                4);
  r |= writedata(pFile, &pConfig->numLoops,                2);
  return (r ? 1 : 0);
}

static int write_frameconfig(FILE* pFile, const CGIFrgb_FrameConfig* pConfig, uint32_t sizeImageData) {
  int r;
  const uint8_t fmtChan = pConfig->fmtChan;
  // attrFlags  : U16
  // genFlags   : U16
  // delay      : U16
  // fmtChan    : U8
  // pImageData : U8[numBytes]
  r  = writedata(pFile, &pConfig->attrFlags,              2);
  r |= writedata(pFile, &pConfig->genFlags,               2);
  r |= writedata(pFile, &pConfig->delay,                  2);
  r |= writedata(pFile, &fmtChan,                         1);
  r |= writedata(pFile, pConfig->pImageData, sizeImageData);
  return (r ? 1 : 0);
}

/* wrapper functions for cgifs API */
CGIFrgb* cgif_rgb_newgif(const CGIFrgb_Config* pConfig) {
  CGIFrgb* pGIF;
  FILE*    pFile;
  int      r;

  pFile = fopen(CGIF_OUTPATH, "wb");
  // check for errors
  if(pFile == NULL) {
    return NULL;
  }
  r = write_gifconfig(pFile, pConfig);
  // check for write errors
  if(r) {
    fclose(pFile);
    return NULL;
  }
  pGIF = malloc(sizeof(CGIFrgb));
  // check for alloc errors
  if(pGIF == NULL) {
    fclose(pFile);
    return NULL;
  }
  pGIF->config    = *pConfig;
  pGIF->pFile     = pFile;
  pGIF->curResult = CGIF_OK;
  return pGIF;
}

cgif_result cgif_rgb_addframe(CGIFrgb* pGIF, const CGIFrgb_FrameConfig* pConfig) {
  int            r;
  const uint32_t sizeImageData = (uint32_t)pGIF->config.width * (uint32_t)pGIF->config.height * pConfig->fmtChan;

  r = write_frameconfig(pGIF->pFile, pConfig, sizeImageData);
  if(r) {
    pGIF->curResult = CGIF_ERROR;
  }
  return CGIF_OK;
}

int cgif_rgb_close(CGIFrgb* pGIF) {
  const int result = pGIF->curResult;

  fclose(pGIF->pFile);
  free(pGIF);
  return result;
}
