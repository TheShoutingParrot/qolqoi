#ifndef _QOLQOI_H
#define _QOLQOI_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// constants
#define CHANNEL_RGB 3
#define CHANNEL_RGBA 4

#define PX (desc.width * desc.height)

// macros
#define PREV_PIXEL_HASH(pixel) (pixel.r * 3 + pixel.g * 5 + pixel.b * 7 + pixel.a * 11) % 64
#define BIGENDIANWRITEBYTESFROM32BIT(f, b, i) b[0] = i >> 24 & 0xFF; b[1] = i >> 16 & 0xFF; b[2] = i >> 8 & 0xFF; b[3] = i & 0xFF; fwrite(b, 1, 4, f)
#define EQPIXELS(p1, p2) p1.r == p2.r && p1.g == p2.g && p1.b == p2.b && p1.a == p2.a

// structs & types
typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
} qoi_desc_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} qoi_pixel_t;

// functions

bool qoiReadHeader(FILE *file, qoi_desc_t *desc);
uint8_t *qoiDecodeFile(const char *fname, qoi_desc_t *desc);
bool qoiEncodeFile(const char *fname, const qoi_desc_t desc, const uint8_t *pixels);

#endif // _QOLQOI_H0b1111010b111101