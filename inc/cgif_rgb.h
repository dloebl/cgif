#ifndef CGIF_RGB_H
#define CGIF_RGB_H

// EXPERIMENTAL: the RGB API is experimental and subject to change.
// It is not part of the stable cgif API and must be enabled explicitly
// via the 'experimental_rgb' build option. The declarations below used to
// live in cgif.h but have been moved out of the regular header.

#include <stdint.h>

#include "cgif.h"

#ifdef __cplusplus
extern "C" {
#endif

// flags to set the RGB frame-attributes
#define CGIF_RGB_FRAME_ATTR_INTERLACED   (1ul << 0)       // encode frame interlaced (default is not interlaced)
#define CGIF_RGB_FRAME_ATTR_NO_DITHERING (1ul << 1)       // disable color dithering (default is with dithering)

typedef enum {
  CGIF_CHAN_FMT_RGB  = 3, // 3 byte per pixel (red, green, blue)
  CGIF_CHAN_FMT_RGBA = 4, // 4 byte per pixel (red, green, blue, alpha)
} cgif_chan_fmt;

typedef struct st_cgif_rgb_config      CGIFrgb_Config;
typedef struct st_cgif_rgb             CGIFrgb;
typedef struct st_cgif_rgb_frameconfig CGIFrgb_FrameConfig;

// prototypes
CGIFrgb*    cgif_rgb_newgif    (const CGIFrgb_Config* pConfig);
cgif_result cgif_rgb_addframe  (CGIFrgb* pGIF, const CGIFrgb_FrameConfig* pConfig);
cgif_result cgif_rgb_close     (CGIFrgb* pGIF);

struct st_cgif_rgb_config {
  cgif_write_fn* pWriteFn;
  void*          pContext;
  const char*    path;
  uint32_t       attrFlags;
  uint32_t       genFlags;
  uint16_t       numLoops;
  uint16_t       width;
  uint16_t       height;
};

struct st_cgif_rgb_frameconfig {
  uint8_t* pImageData;
  cgif_chan_fmt fmtChan;
  uint32_t attrFlags;   // TBD
  uint32_t genFlags;    // TBD
  uint16_t delay;
};

#ifdef __cplusplus
}
#endif

#endif // CGIF_RGB_H
