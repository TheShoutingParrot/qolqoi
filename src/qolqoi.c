#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "qolqoi.h"

#define ERR(m) fprintf(stderr, "err in qolqoi (%10s %04d): %s", __FILE__, __LINE__, m)

uint8_t *qoiReadBytes(FILE *file, uint16_t n) {
    uint8_t *bytes = (uint8_t *)malloc(n * sizeof(uint8_t));
    for (uint16_t bi = 0; bi < n; bi++) {
        bytes[bi] = fgetc(file);
    }

    return bytes;
}

// read the qoi header. returns false if not a valid .qoi file
bool qoiReadHeader(FILE *file, qoi_desc_t *desc) {
    int bi;

    char *magicBytes = qoiReadBytes(file, 4);

    if (!(magicBytes[0] == 'q' && magicBytes[1] == 'o' && magicBytes[2] == 'i' && magicBytes[3] == 'f')) {
        printf("wrong magic (got \"%s\" and not qoif)\n", magicBytes);
        free(magicBytes);
        return false;
    }

    free(magicBytes);


    uint8_t *descBytes = qoiReadBytes(file, 10);

    desc->width = (uint32_t)descBytes[0] << 24 | (uint32_t)descBytes[1] << 16 | (uint32_t)descBytes[2] << 8 | (uint32_t)descBytes[3];

    desc->height = (uint32_t)descBytes[4] << 24 | (uint32_t)descBytes[5] << 16 | (uint32_t)descBytes[6] << 8 | (uint32_t)descBytes[7];

    desc->channels = descBytes[8];
    desc->colorspace = descBytes[9];

    return true;
}

// decodes a qoi file and returns bytes representing pixels (first byte red, second green, third blue and possibly fourth alpha)
//
// will do mallocing/etc for you
uint8_t *qoiDecodeFile(const char *fname, qoi_desc_t *desc) {
    FILE *file;
    uint8_t byte;

    file = fopen(fname, "r");
    if (file == NULL) {
        puts("failed to open file");
        return NULL;
    }

    bool b = qoiReadHeader(file, desc);
    if (!b) {
        puts("failed to read header");
        fclose(file);
        return NULL;
    }

    uint8_t channels = desc->channels;

    size_t pixelLen = ((size_t)desc->width * (size_t)desc->height) * (size_t)channels;
    uint8_t *pixels;

    if (channels == CHANNEL_RGBA){
        pixels = (uint8_t *)malloc(pixelLen * 4);
    } else {
        pixels = (uint8_t *)malloc(pixelLen * 3);
    }

    qoi_pixel_t prevPixel = {r : 0, g : 0, b : 0, a : 255};
    qoi_pixel_t *prevPixelArr =
        (qoi_pixel_t *)calloc(64, sizeof(qoi_pixel_t));

    uint8_t currentRun = 0;
    uint8_t endCount = 0;

    for (uint64_t pixelIndex = 0; pixelIndex < pixelLen;) {
        if (currentRun) {
            currentRun--;
        } else {
            uint8_t *bytes;
            bytes = qoiReadBytes(file, 1);

            if (!(*bytes & 0b11000000)) {
                // first 2 bits 0 -> qoi_op_index
                prevPixel = prevPixelArr[(*bytes & 0b00111111)];

                free(bytes);
            } else if (!(*bytes & 0b10000000)) { // first bit 0 -> qoi_op_diff
                // 00 -> -2
                // 01 -> -1
                // 11 -> 1

                prevPixel.r += ((int8_t)((*bytes & 0b00110000) >> 4)) - 2;
                prevPixel.g += ((int8_t)((*bytes & 0b00001100) >> 2)) - 2;
                prevPixel.b += ((int8_t)((*bytes & 0b00000011))) - 2;

                free(bytes);
            } else if (!(*bytes & 0b01000000)) { // second bit 0 -> qoi_op_luma
                // 0b000000 -> -32
                // 0b000001 -> -31, etc...
                int8_t greendiff = ((int8_t)(*bytes & 0b00111111)) - 32;

                free(bytes);

                bytes = qoiReadBytes(file, 1);

                prevPixel.g += greendiff;
                prevPixel.r += (greendiff + (int8_t)((*bytes & 0b11110000) >> 4)) - 8;
                prevPixel.b += (greendiff + (int8_t)(*bytes & 0b00001111)) - 8;

                free(bytes);
            } else if (*bytes == 0xFF) { // all bits are 1 -> qoi_op_rgba
                free(bytes);

                bytes = qoiReadBytes(file, 4);

                prevPixel.r = bytes[0];
                prevPixel.g = bytes[1];
                prevPixel.b = bytes[2];
                prevPixel.a = bytes[3];

                free(bytes);
            } else if (*bytes == 0xFE) { // *bytes == 0xFE -> qoi_op_rgb
                free(bytes);

                bytes = qoiReadBytes(file, 3);

                prevPixel.r = bytes[0];
                prevPixel.g = bytes[1];
                prevPixel.b = bytes[2];

                free(bytes);
            } else { // this means that first two bits are set -> qoi_op_run
                currentRun = 1 + ((uint8_t)*bytes) & 0b00111111;
                free(bytes);
                goto skip_pixel_stuff; 
            }

            prevPixelArr[PREV_PIXEL_HASH(prevPixel)] = prevPixel;
        }

        pixels[pixelIndex] = prevPixel.r;
        pixels[pixelIndex + 1] = prevPixel.g;
        pixels[pixelIndex + 2] = prevPixel.b;

        if (channels == CHANNEL_RGBA) {
            pixels[pixelIndex + 3] = prevPixel.a;
        }

        pixelIndex += channels;

    skip_pixel_stuff:
    }

endloop:

    fclose(file);

    return pixels;
}

qoi_pixel_t bytesToQOIPixel(const uint8_t *data, uint64_t i, uint8_t channels) {
    qoi_pixel_t pixel = {r: data[i], g: data[i+1], b: data[i+2], a: 255};
    if (channels == CHANNEL_RGBA) {
        pixel.a = data[i+3];
    }

    return pixel;
}

bool qoiEncodeFile(const char *fname, const qoi_desc_t desc, const uint8_t *pixels) {
    FILE *file = fopen(fname, "w");
    if (file == NULL) {
        puts("failed to open file");
        return false;
    }

    // header
    // write magic

    if (fwrite("qoif", 1, 4, file) != 4)
        return false;

    // width & height (must make sure that written in big endian)
    uint8_t *bytes = malloc(4);

    BIGENDIANWRITEBYTESFROM32BIT(file, bytes, desc.width);
    BIGENDIANWRITEBYTESFROM32BIT(file, bytes, desc.height);

    // write channels and colorspace
    bytes[0] = desc.channels;
    bytes[1] = desc.colorspace;
    fwrite(bytes, 1, 2, file);

    free(bytes);

    qoi_pixel_t prevPixel = {r : 0, g : 0, b : 0, a : 255};
    qoi_pixel_t *prevPixelArr =
        (qoi_pixel_t *)calloc(64, sizeof(qoi_pixel_t));
    bool *prevPixelExistsArr =
        (bool *)calloc(64, sizeof(bool));

    const uint64_t pixelLen = PX * 4;//desc.width * desc.height * desc.channels;

    // theoretically the longest possible run is as much as pixelLen is 
    uint64_t currentRun = 0;

    qoi_pixel_t currentPixel;

    for(uint64_t pixelIndex = 0; pixelIndex < pixelLen || currentRun; pixelIndex+=desc.channels) {
        // if a "run" is still active we zero it
        if (currentRun && !(pixelIndex < pixelLen)) {
            uint8_t wb;

            // if pixels weren't the same but run length is not 0 than
            // a run has "concluded" and we can write the QOI_OP_RUN
            wb = 0b11000000 | (currentRun - 1);
            fwrite(&(wb), 1, 1, file);
            
            break;
        }

        currentPixel = bytesToQOIPixel(pixels, pixelIndex, desc.channels);

        // if the pixel is the same as prev, then increase the run length
        if(EQPIXELS(currentPixel, prevPixel)) {
            // maximum length is 62 so we write it and lessen by 62 if run L is already over the max
            if (++currentRun > 62) {
                uint8_t wb;
                wb = 0b11111101; // maximum length as 0b11000000 & (62 - 1)
                fwrite(&(wb), 1, 1, file);

                currentRun-=62;
            }

            continue;
        } else if (currentRun) {
            uint8_t wb;

            // if pixels weren't the same but run length is not 0 than
            // a run has "concluded" and we can write the QOI_OP_RUN
            wb = 0b11000000 | (currentRun - 1);
            fwrite(&(wb), 1, 1, file);

skipfinalrunwrite:

            currentRun = 0;

            if (!(pixelIndex < pixelLen)) {
                break;
            }
        }

        // if we've seen this pixel before than use the index (except if collision)
        uint8_t pixelHash = PREV_PIXEL_HASH(currentPixel);
        if (prevPixelExistsArr[pixelHash]) {
            // check that the hashed one is actually the one we want
            if (EQPIXELS(prevPixelArr[pixelHash], currentPixel)) {
                fwrite(&pixelHash, 1, 1, file);
                
                prevPixel = currentPixel;
                
                continue;
            }

            // if not and there is a collision, we must continue on our quest
        }

        // add to the array
        prevPixelExistsArr[pixelHash] = true;
        prevPixelArr[pixelHash] = currentPixel;

        // if the alpha value changed than we must do OP_RGBA
        if (currentPixel.a != prevPixel.a) {
            bytes = malloc(5);

            bytes[0] = 0xFF;
            bytes[1] = currentPixel.r;
            bytes[2] = currentPixel.g;
            bytes[3] = currentPixel.b;
            bytes[4] = currentPixel.a;

            fwrite(bytes, 1, 5, file);

            free(bytes);
        } else {
            // check if we can do qoi_op_diff
            int8_t dr, dg, db;
            dr = (int8_t)(currentPixel.r - prevPixel.r);
            dg = (int8_t)(currentPixel.g - prevPixel.g);
            db = (int8_t)(currentPixel.b - prevPixel.b);

            if(-2 <= dr && dr <= 1 && -2 <= dg && dg <= 1 && -2 <= db && db <= 1) {
                uint8_t wb = 0b01000000 | (((uint8_t)dr + 2) << 4) | (((uint8_t)dg + 2) << 2) | ((uint8_t)db + 2);
                fwrite(&wb, 1, 1, file);
            } else {
                 // check if we can do op_luma instead
                const int8_t dr_dg = dr - dg;
                const int8_t db_dg = db - dg;
                if (-32 <= dg && dg <= 31 && -8 <= dr_dg && dr_dg <= 7 && -8 <= db_dg && db_dg <= 7) {
                    bytes = malloc(2);

                    bytes[0] = 0b10000000 | ((uint8_t)dg+32);
                    bytes[1] = ((uint8_t)dr_dg+8) << 4 | ((uint8_t)db_dg+8);

                    fwrite(bytes, 1, 2, file);

                    free(bytes);
                } else {
                    // at this point it is clear that we must do qoi_op_rgb

                    bytes = malloc(4);

                    bytes[0] = 0xFE;
                    bytes[1] = currentPixel.r;
                    bytes[2] = currentPixel.g;
                    bytes[3] = currentPixel.b;

                    fwrite(bytes, 1, 4, file);

                    free(bytes);
                }
            }
        } 

        prevPixel = currentPixel;
    }

    free(prevPixelArr);
    free(prevPixelExistsArr);

    const uint8_t endBytes[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    fwrite(endBytes, 1, 8, file);

    fclose(file);

    return true;
}

/*int main(int argc, char **argv) {
    qoi_desc_t desc;
    uint8_t *pixels = qoiDecodeFile("baboon.qoi", &desc);
    printf("desc: %d %d %d %d\n", desc.height, desc.width, desc.channels, desc.colorspace);
    printf("pixel: %p\n", pixels);

    bool f = qoiEncodeFile("new.qoi", desc, pixels);
    printf("not failed: %d\n", f);

    if(argc != 2) {
        return 0;
    }

    uint64_t pixlen = PX * 4;//desc.width*desc.height*desc.channels;
    for  (uint64_t pixi = 0; pixi < pixlen; pixi+=desc.channels) {
        if ((pixi/desc.channels) % desc.width == 0) {
            putchar('\n');
        }
        printf("\033[48;2;%d;%d;%dm%ld\033[0m", pixels[pixi], pixels[pixi + 1], pixels[pixi + 2], (pixi/desc.channels)%10);
    }

    putchar('\n');

    free(pixels);
    printf("hello\n");
    pixels = qoiDecodeFile("new.qoi", &desc);
    printf("hello %p", pixels);

    pixlen = PX * 4;//desc.width*desc.height*desc.channels;
    for  (uint64_t pixi = 0; pixi < pixlen; pixi+=desc.channels) {
        if ((pixi/desc.channels) % desc.width == 0) {
            putchar('\n');
        }
        printf("\033[48;2;%d;%d;%dm%ld\033[0m", pixels[pixi], pixels[pixi + 1], pixels[pixi + 2], (pixi/desc.channels)%10);
    }

    putchar('\n');
    
    free(pixels);

    return 0;
}*/