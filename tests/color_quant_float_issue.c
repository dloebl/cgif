#include <cgif.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static int writecb(void* pContext, const uint8_t* pData, size_t size) {
    (void)pContext; (void)pData; (void)size;
    return 0;
}

int main() {
    /*
     * before treeNode.mean in cgif_rgb.c was changed from float to double: Heap buffer over-read in cgif_rgb.c crawl_decision_tree()
     *
     * Float precision bug in mean-cut color quantization.
     * When dominant color (246,0,0) has freq 545600 and rare color
     * (247,0,0) has freq 1, the float mean computation:
     *   mean = (float)(545600*246 + 1*247) / (float)(545600+1)
     *        = (float)(134217847) / (float)(545601)
     *        = 134217840.0 / 545601.0  (numerator lost precision!)
     *        = 245.999985 < 246
     *
     * This causes the partition loop in crawl_decision_tree to never
     * advance, producing a child node with invalid range
     * [idxMin, idxMin-1] = [idxMin, UINT32_MAX], causing a massive
     * heap buffer over-read in get_mean/get_variance.
     *
     * Dimensions: 192 x 2843 = 545856 pixels
     * 255 cloud colors + 1 rare + 545600 dominant = 545856
     */

    uint16_t w = 192;
    uint16_t h = 2843;
    uint32_t numPixel = (uint32_t)w * h;

    fprintf(stderr, "Image: %ux%u = %u pixels\n", w, h, numPixel);

    uint8_t* imgData = malloc(numPixel * 3);
    if (!imgData) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    uint32_t idx = 0;

    /* 255 cloud colors: R=246, G=1..255, B=0 (each appears 1x)
     * These share R=246 with the dominant color so they'll be
     * in the same partition region along the R dimension.
     * Total unique colors = 255 + 1 + 1 = 257 > 255, forcing quantization.
     */
    for (int g = 1; g <= 255; g++, idx++) {
        imgData[idx * 3 + 0] = 246;
        imgData[idx * 3 + 1] = g;
        imgData[idx * 3 + 2] = 0;
    }

    /* 1 pixel of rare color: R=247, G=0, B=0 */
    imgData[idx * 3 + 0] = 247;
    imgData[idx * 3 + 1] = 0;
    imgData[idx * 3 + 2] = 0;
    idx++;

    /* Remaining 545600 pixels: dominant color R=246, G=0, B=0 */
    uint32_t dominant_freq = numPixel - idx;
    fprintf(stderr, "Dominant color (246,0,0) freq: %u (need exactly 545600)\n", dominant_freq);
    fprintf(stderr, "Rare color (247,0,0) freq: 1\n");
    fprintf(stderr, "Cloud colors (246,1-255,0): 255\n");
    fprintf(stderr, "Total unique colors: 257\n");

    for (; idx < numPixel; idx++) {
        imgData[idx * 3 + 0] = 246;
        imgData[idx * 3 + 1] = 0;
        imgData[idx * 3 + 2] = 0;
    }

    /* Verify the float precision bug condition */
    float num = (float)((uint64_t)545600 * 246 + (uint64_t)1 * 247);
    float den = (float)(545600 + 1);
    float mean = num / den;
    fprintf(stderr, "\nFloat precision check:\n");
    fprintf(stderr, "  mean = %.15f (should be >= 246.0)\n", mean);
    fprintf(stderr, "  mean < 246.0? %s\n", mean < 246.0f ? "YES - BUG WILL TRIGGER" : "NO");

    /* Create GIF */
    CGIFrgb_Config config = {0};
    config.width = w;
    config.height = h;
    config.pWriteFn = writecb;

    CGIFrgb* pGIF = cgif_rgb_newgif(&config);
    if (!pGIF) {
        fprintf(stderr, "cgif_rgb_newgif failed\n");
        free(imgData);
        return 1;
    }

    CGIFrgb_FrameConfig fconfig = {0};
    fconfig.pImageData = imgData;
    fconfig.fmtChan = CGIF_CHAN_FMT_RGB;
    fconfig.delay = 10;

    fprintf(stderr, "\nCalling cgif_rgb_addframe...\n");
    cgif_result r = cgif_rgb_addframe(pGIF, &fconfig);
    fprintf(stderr, "Result: %d\n", r);

    r = cgif_rgb_close(pGIF);
    free(imgData);
    return 0;
}
