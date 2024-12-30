# qolqoi

Quite Ok Library for QOI

Library for encoding and decoding qoi files.

[Read more about qoi files.](https://qoiformat.org/)

This library was made by me as a little project on my freetime. Done by staring at the [qoi specification pdf](https://qoiformat.org/qoi-specification.pdf).

## Library usage

The only essential structure provided by qolqoi is the following:

```c
typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
} qoi_desc_t;
```

This struct stores the essential info about the file. Same data stored in header.

### Functions

Provides 3 essential functions:

#### read header

```c
bool qoiReadHeader(FILE *file, qoi_desc_t *desc)
```

Returns false if it fails to read the header. Reads the header of the file (checks magic bytes) and puts the data in desc.

#### decode file

```c
uint8_t *decodeQOIFile(const char *fname, qoi_desc_t *desc);
```

Returns raw pixels as bytes of qoi file by the name of fname. Also will put header data in desc.

Raw pixels will either be rgb values (each color with a separate byte) or rgba values (so for 4 bytes per pixel).

Will return `NULL` on failure.

#### encode file

```c
bool encodeQOIFile(const char *fname, const qoi_desc_t desc, const uint8_t *pixels);
```

Creates a new file by the name of fname and encodes to that file the given pixels. Will return false on failure. Requires a valid given `qoi_desc_t desc`.

### Example project

[examples/conv.c](examples/conv.c) is a simple png to qoi and qoi to png converter. Uses this library.

To run it first run `make install` and then `make examples` to complile the example. Then you can run it: `./examples/conv imgs/baboon.qoi output.png`.

## Installation

`make` will simply compile libqolqoi.so.

`make install` will install the library to /usr/lib (by default).

After installing it you may use the library by adding the -lqolqoi flag to your c compiler.