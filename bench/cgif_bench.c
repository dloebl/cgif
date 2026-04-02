#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "cgif.h"

#define WIDTH  8192
#define HEIGHT 8192

static uint64_t seed = 42;

__attribute__((no_sanitize("integer")))
static int psdrand(void) {
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

static void encode(const char *path, uint8_t *palette, uint16_t numColors, uint8_t *pImageData) {
  CGIF_Config gConfig;
  CGIF_FrameConfig fConfig;

  memset(&gConfig, 0, sizeof(gConfig));
  gConfig.width = WIDTH;
  gConfig.height = HEIGHT;
  gConfig.pGlobalPalette = palette;
  gConfig.numGlobalPaletteEntries = numColors;
  gConfig.path = path;

  CGIF *pGIF = cgif_newgif(&gConfig);
  memset(&fConfig, 0, sizeof(fConfig));
  fConfig.pImageData = pImageData;
  cgif_addframe(pGIF, &fConfig);
  cgif_close(pGIF);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <stripe|gradient|noise|solid>\n", argv[0]);
    return 1;
  }

  uint8_t palette256[256 * 3];
  for (int i = 0; i < 256; ++i) {
    palette256[i * 3]     = i;
    palette256[i * 3 + 1] = i;
    palette256[i * 3 + 2] = i;
  }

  uint8_t palette3[] = {0xFF,0x00,0x00, 0x00,0xFF,0x00, 0x00,0x00,0xFF};
  uint8_t palette4[] = {0xFF,0x00,0x00, 0x00,0xFF,0x00, 0x00,0x00,0xFF, 0xFF,0xFF,0x00};
  uint8_t *img = malloc(WIDTH * HEIGHT);

  if (strcmp(argv[1], "stripe") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = (i % WIDTH) / 4 % 3;
    encode("/dev/null", palette3, 3, img);

  } else if (strcmp(argv[1], "gradient") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = (i % WIDTH) * 256 / WIDTH;
    encode("/dev/null", palette256, 256, img);

  } else if (strcmp(argv[1], "noise") == 0) {
    seed = 42;
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = psdrand() % 256;
    encode("/dev/null", palette256, 256, img);

  } else if (strcmp(argv[1], "solid") == 0) {
    memset(img, 0, WIDTH * HEIGHT);
    encode("/dev/null", palette3, 3, img);

  } else if (strcmp(argv[1], "checker") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = (((i % WIDTH) + (i / WIDTH)) & 1);
    encode("/dev/null", palette3, 3, img);

  } else if (strcmp(argv[1], "dither") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = ((i % WIDTH) + (i / WIDTH) * 7) % 256;
    encode("/dev/null", palette256, 256, img);

  } else if (strcmp(argv[1], "fewnoise") == 0) {
    seed = 42;
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
      img[i] = psdrand() % 4;
    encode("/dev/null", palette4, 4, img);

  } else {
    fprintf(stderr, "unknown pattern: %s\n", argv[1]);
    free(img);
    return 1;
  }

  free(img);
  return 0;
}
