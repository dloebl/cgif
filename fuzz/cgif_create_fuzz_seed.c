/*
  Create seed corpus from our test cases (tests/).
  Wrap cgifs API and write configuration to CGIF_OUTPATH.
*/

#include <cgif.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef CGIF_OUTPATH
  #error "please set a output path: define CGIF_OUTPATH"
#endif

typedef struct st_gif {
  CGIF_Config config;
  FILE*       pFile;
  cgif_result curResult;
} CGIF;

static int writedata(FILE* pFile, const void* pData, uint32_t size) {
  int r = fwrite(pData, size, 1, pFile);
  if(r != 1) return 1;
  return 0;
}

static int write_gifconfig(FILE* pFile, const CGIF_Config* pConfig) {
  int r;
  // width     : U16
  // height    : U16
  // attrFlags : U32
  // genFlags  : U32
  // sizeGCT   : U16
  // numLoops  : U16
  // pGCT      : U8[sizeGCT * 3]
  r  = writedata(pFile, &pConfig->width,                   2);
  r |= writedata(pFile, &pConfig->height,                  2);
  r |= writedata(pFile, &pConfig->attrFlags,               4);
  r |= writedata(pFile, &pConfig->genFlags,                4);
  r |= writedata(pFile, &pConfig->numGlobalPaletteEntries, 2);
  if(!(pConfig->attrFlags & CGIF_ATTR_NO_GLOBAL_TABLE)) {
    r |= writedata(pFile, pConfig->pGlobalPalette, pConfig->numGlobalPaletteEntries * 3);
  }
  return (r ? 1 : 0);
}

static int write_frameconfig(FILE* pFile, const CGIF_FrameConfig* pConfig, uint32_t sizeImageData) {
  int r;
  // attrFlags  : U16
  // genFlags   : U16
  // delay      : U16
  // transIndex : U8
  // sizeLCT    : U16
  // pLCT       : U8[sizeLCT * 3]
  // pImageData : U8[numBytes]
  r  = writedata(pFile, &pConfig->attrFlags,              2);
  r |= writedata(pFile, &pConfig->genFlags,               2);
  r |= writedata(pFile, &pConfig->delay,                  2);
  r |= writedata(pFile, &pConfig->transIndex,             1);
  r |= writedata(pFile, &pConfig->numLocalPaletteEntries, 2);
  if(pConfig->attrFlags & CGIF_FRAME_ATTR_USE_LOCAL_TABLE) {
    r |= writedata(pFile, pConfig->pLocalPalette, pConfig->numLocalPaletteEntries * 3);
  }
  r |= writedata(pFile, pConfig->pImageData, sizeImageData);
  return (r ? 1 : 0);
}

/* wrapper functions for cgifs API */
CGIF* cgif_newgif(CGIF_Config* pConfig) {
  CGIF* pGIF;
  FILE* pFile;
  int   r;

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
  pGIF = malloc(sizeof(CGIF));
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

int cgif_addframe(CGIF* pGIF, CGIF_FrameConfig* pConfig) {
  int            r;
  const uint32_t sizeImageData = (uint32_t)pGIF->config.width * (uint32_t)pGIF->config.height;

  r = write_frameconfig(pGIF->pFile, pConfig, sizeImageData);
  if(r) {
    pGIF->curResult = CGIF_ERROR;
  }
  return CGIF_OK;
}

int cgif_close(CGIF* pGIF) {
  const int result = pGIF->curResult;

  fclose(pGIF->pFile);
  free(pGIF);
  return result;
}

int __real_main(void);
int __wrap_main(void) {
  __real_main();
  return 0;
}
