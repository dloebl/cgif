#pragma once

#include <stddef.h>
#include <stdint.h>

static inline int cgif_size_mul(size_t a, size_t b, size_t* out) {
    if (!out) return -1;
    if (a != 0 && b > (SIZE_MAX / a)) return -1;
    *out = a * b;
    return 0;
}

static inline int cgif_size_mul3(size_t a, size_t b, size_t c, size_t* out) {
    size_t tmp;
    if (cgif_size_mul(a, b, &tmp) != 0) return -1;
    return cgif_size_mul(tmp, c, out);
}
