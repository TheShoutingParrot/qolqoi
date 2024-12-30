#include "qolqoi.h"
#include <png.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define CHECK_FNAME_EXT(str, end) (strcmp((str+strlen(str) - 4), end) == 0)
#define PRINT_USAGE fprintf(stderr, "usage: %s [input png file or qoi file] [output qoi file or png file]\nNOTE: Files must have proper filename extensions!\n", argv[0])

// Structure to hold PNG image details and pixel data
typedef struct {
    int width;
    int height;
    int color_type;
    int bit_depth;
    png_bytep *row_pointers; // Array of row pointers for pixel data
} _PNGImage;

int savePNG(const char *filename, uint8_t *raw_data, uint32_t width, uint32_t height, uint8_t channel) {
    int chan;
    if (channel == CHANNEL_RGBA) {
        chan = PNG_COLOR_TYPE_RGB_ALPHA;
    } else {
        chan = PNG_COLOR_TYPE_RGB;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open file");
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Failed to create PNG write struct\n");
        fclose(fp);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Failed to create PNG info struct\n");
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error during PNG creation\n");
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);

    // Write the header
    png_set_IHDR(
        png, info,
        width, height,
        8,                
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    // Prepare row pointers
    png_bytep row_pointers[height];
    for (int y = 0; y < height; y++) {
        row_pointers[y] = raw_data + y * width * 4;
    }

    // Write image data
    png_write_image(png, row_pointers);

    // End write
    png_write_end(png, NULL);

    // Clean up
    png_destroy_write_struct(&png, &info);
    fclose(fp);

    return 0;
}

// Function to read PNG file and load data into a PNGImage structure
int readPNG(const char *filename, _PNGImage *image) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Failed to open file");
        return -1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fputs("Failed to create PNG read struct\n", stderr);
        fclose(fp);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fputs("Failed to create PNG info struct\n", stderr);
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fputs("Error during PNG reading", stderr);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    // Read image header
    image->width = png_get_image_width(png, info);
    image->height = png_get_image_height(png, info);
    image->color_type = png_get_color_type(png, info);
    image->bit_depth = png_get_bit_depth(png, info);

    // Transformations to ensure consistent pixel format
    if (image->bit_depth == 16) {
        png_set_strip_16(png); // Convert 16-bit to 8-bit
    }
    if (image->color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png); // Convert palette to RGB
    }
    if (image->color_type == PNG_COLOR_TYPE_GRAY && image->bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png); // Expand grayscale to 8-bit
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png); // Add alpha channel if transparency exists
    }
    if (image->color_type == PNG_COLOR_TYPE_RGB || image->color_type == PNG_COLOR_TYPE_GRAY) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER); // Add full alpha channel
    }

    png_read_update_info(png, info);

    // Allocate memory for row pointers
    image->row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * image->height);
    for (int y = 0; y < image->height; y++) {
        image->row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }

    // Read the image data
    png_read_image(png, image->row_pointers);

    // Clean up
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 0;
}

// Free the allocated memory for pixel data
void freePNG(_PNGImage *image) {
    for (int y = 0; y < image->height; y++) {
        free(image->row_pointers[y]);
    }
    free(image->row_pointers);
}


bool convertQOItoPNG(const char *inputfname, const char *outputfname) {
    qoi_desc_t qoiDesc;
    uint8_t *pixels;
    
    pixels = qoiDecodeFile(inputfname, &qoiDesc);
    if (pixels == NULL) {
        fputs("failed to decode qoi file", stderr);
        return false;
    }
    
    if (savePNG(outputfname, pixels, qoiDesc.width, qoiDesc.height, qoiDesc.channels) != 0) {
        fputs("failed to encode png file", stderr);
        return false;
    }

    return true;
}

bool convertPNGtoQOI(const char *inputfname, const char *outputfname) {
    qoi_desc_t qoiDesc;
    uint8_t *pixels;
    _PNGImage img;

    if (readPNG(inputfname, &img) != 0) {
        fputs("failed to decode png file", stderr);
        return false;
    }

    qoiDesc.channels = 4;
    qoiDesc.colorspace = 0;
    qoiDesc.width = (uint32_t)img.width;
    qoiDesc.height = (uint32_t)img.height;

    pixels = malloc(qoiDesc.width * qoiDesc.height * 4);
    
    uint64_t pixi = 0;
    for (uint64_t y = 0; y < qoiDesc.height; y++) {
        for (uint64_t x = 0; x < ((uint64_t)qoiDesc.width)*4; x+=4) {
            pixels[pixi] = img.row_pointers[y][x];
            pixels[pixi+1] = img.row_pointers[y][x+1];
            pixels[pixi+2] = img.row_pointers[y][x+2];
            pixels[pixi+3] = img.row_pointers[y][x+3];

            pixi+=4;
        }
    }
    
    freePNG(&img);

    if (!qoiEncodeFile(outputfname, qoiDesc, pixels)) {
        fputs("failed to encode qoi file", stderr);
        return false;
    }

    free(pixels);

    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        PRINT_USAGE;
        exit(1);
    }

    if (CHECK_FNAME_EXT(argv[1], ".qoi") && CHECK_FNAME_EXT(argv[2], ".png")) {
        if(!convertQOItoPNG(argv[1], argv[2])) {
            fputs("failed to do conversion qoi->png\n", stderr);
            exit(1);
        }
    } else if (CHECK_FNAME_EXT(argv[1], ".png") && CHECK_FNAME_EXT(argv[2], ".qoi")) {
        if(!convertPNGtoQOI(argv[1], argv[2])) {
            fputs("failed to do conversion png->qoi\n", stderr);
            exit(1);
        }
    } else {
        PRINT_USAGE;
        exit(1);
    }

    printf("successfully converted file %s to %s\n", argv[1], argv[2]);

    return 0;
}