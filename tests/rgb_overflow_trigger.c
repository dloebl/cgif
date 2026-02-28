#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgif.h"

static int sink_write(void* pContext, const uint8_t* pData, size_t numBytes) {
  (void)pContext;
  (void)pData;
  (void)numBytes;
  return 0;
}

static void sig_handler(int sig) {
  fprintf(stderr, "signal=%d\n", sig);
  fflush(stderr);
  _Exit(128 + sig);
}

int main(void) {
  const uint16_t width = 32768;
  const uint16_t height = 32769;

  signal(SIGSEGV, sig_handler);
  signal(SIGABRT, sig_handler);
  signal(SIGBUS, sig_handler);

  CGIFrgb_Config gConfig = {0};
  gConfig.pWriteFn = sink_write;
  gConfig.width = width;
  gConfig.height = height;

  CGIFrgb* pGIF = cgif_rgb_newgif(&gConfig);
  if(pGIF == NULL) {
    printf("newgif_failed\n");
    return 2;
  }

  size_t logical = (size_t)width * height * 4;
  uint8_t* buf = malloc(logical);
  if(buf == NULL) {
    printf("malloc_failed\n");
    cgif_rgb_close(pGIF);
    return 3;
  }
  memset(buf, 0, logical);

  CGIFrgb_FrameConfig fConfig = {0};
  fConfig.pImageData = buf;
  fConfig.fmtChan = CGIF_CHAN_FMT_RGBA;
  fConfig.delay = 0;

  cgif_result addResult = cgif_rgb_addframe(pGIF, &fConfig);
  printf("addframe_rc=%d\n", addResult);

  cgif_result closeResult = cgif_rgb_close(pGIF);
  printf("close_rc=%d\n", closeResult);

  free(buf);
  if(addResult == CGIF_EALLOC && closeResult != CGIF_OK) {
    return 0;
  }
  return 1;
}
