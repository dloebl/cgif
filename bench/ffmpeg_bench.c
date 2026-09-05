#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>

#define WIDTH  8192
#define HEIGHT 8192

static uint64_t seed = 42;

__attribute__((no_sanitize("integer")))
static int psdrand(void) {
  seed = 6364136223846793005ULL * seed + 1;
  return seed >> 33;
}

static void encode(const char *path, uint32_t *palette, uint8_t *img) {
  const AVCodec *codec = avcodec_find_encoder_by_name("gif");
  AVCodecContext *ctx = avcodec_alloc_context3(codec);
  ctx->width = WIDTH;
  ctx->height = HEIGHT;
  ctx->pix_fmt = AV_PIX_FMT_PAL8;
  ctx->time_base = (AVRational){1, 25};
  avcodec_open2(ctx, codec, NULL);

  AVFrame *frame = av_frame_alloc();
  frame->format = AV_PIX_FMT_PAL8;
  frame->width = WIDTH;
  frame->height = HEIGHT;
  frame->pts = 0;
  av_frame_get_buffer(frame, 0);

  memcpy(frame->data[1], palette, 256 * 4);
  for (int y = 0; y < HEIGHT; ++y)
    memcpy(frame->data[0] + y * frame->linesize[0], img + y * WIDTH, WIDTH);

  AVPacket *pkt = av_packet_alloc();
  avcodec_send_frame(ctx, frame);
  avcodec_send_frame(ctx, NULL);

  FILE *f = fopen(path, "wb");
  while (avcodec_receive_packet(ctx, pkt) == 0) {
    fwrite(pkt->data, 1, pkt->size, f);
    av_packet_unref(pkt);
  }
  fclose(f);

  av_packet_free(&pkt);
  av_frame_free(&frame);
  avcodec_free_context(&ctx);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <stripe|gradient|noise|solid>\n", argv[0]);
    return 1;
  }

  uint32_t palette256[256];
  for (int i = 0; i < 256; ++i)
    palette256[i] = 0xFF000000 | (i << 16) | (i << 8) | i;

  uint32_t palette3[256] = {0};
  palette3[0] = 0xFF0000FF;
  palette3[1] = 0xFF00FF00;
  palette3[2] = 0xFFFF0000;

  uint32_t palette4[256] = {0};
  palette4[0] = 0xFF0000FF;
  palette4[1] = 0xFF00FF00;
  palette4[2] = 0xFFFF0000;
  palette4[3] = 0xFF00FFFF;

  uint8_t *img = malloc(WIDTH * HEIGHT);

  if (strcmp(argv[1], "stripe") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = (i % WIDTH) / 4 % 3;
    encode("/dev/null", palette3, img);
  } else if (strcmp(argv[1], "gradient") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = (i % WIDTH) * 256 / WIDTH;
    encode("/dev/null", palette256, img);
  } else if (strcmp(argv[1], "noise") == 0) {
    seed = 42;
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = psdrand() % 256;
    encode("/dev/null", palette256, img);
  } else if (strcmp(argv[1], "solid") == 0) {
    memset(img, 0, WIDTH * HEIGHT);
    encode("/dev/null", palette3, img);
  } else if (strcmp(argv[1], "checker") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = (((i % WIDTH) + (i / WIDTH)) & 1);
    encode("/dev/null", palette3, img);
  } else if (strcmp(argv[1], "dither") == 0) {
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = ((i % WIDTH) + (i / WIDTH) * 7) % 256;
    encode("/dev/null", palette256, img);
  } else if (strcmp(argv[1], "fewnoise") == 0) {
    seed = 42;
    for (int i = 0; i < WIDTH * HEIGHT; ++i) img[i] = psdrand() % 4;
    encode("/dev/null", palette4, img);
  } else {
    fprintf(stderr, "unknown pattern: %s\n", argv[1]);
    free(img);
    return 1;
  }

  free(img);
  return 0;
}
