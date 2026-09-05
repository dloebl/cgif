#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <gif_lib.h>

#define WIDTH  8192
#define HEIGHT 8192

static uint64_t seed = 42;

__attribute__((no_sanitize("integer")))
static int psdrand(void) {
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

static void encode(const char *path, GifColorType *colors, int numColors, uint8_t *img) {
  int error;
  int mapSize = 1;
  while (mapSize < numColors) mapSize *= 2;

  GifFileType *gif = EGifOpenFileName(path, false, &error);
  ColorMapObject *cmap = GifMakeMapObject(mapSize, NULL);
  memcpy(cmap->Colors, colors, numColors * sizeof(GifColorType));

  EGifSetGifVersion(gif, true);
  EGifPutScreenDesc(gif, WIDTH, HEIGHT, 8, 0, cmap);
  EGifPutImageDesc(gif, 0, 0, WIDTH, HEIGHT, false, NULL);

  for (int y = 0; y < HEIGHT; ++y)
    EGifPutLine(gif, img + y * WIDTH, WIDTH);

  GifFreeMapObject(cmap);
  EGifCloseFile(gif, &error);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <stripe|gradient|noise|solid>\n", argv[0]);
    return 1;
  }

  GifColorType palette256[256];
  for (int i = 0; i < 256; ++i) {
    palette256[i].Red = i; palette256[i].Green = i; palette256[i].Blue = i;
  }

  GifColorType palette3[3] = {{0xFF,0,0},{0,0xFF,0},{0,0,0xFF}};
  GifColorType palette4[4] = {{0xFF,0,0},{0,0xFF,0},{0,0,0xFF},{0xFF,0xFF,0}};
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
