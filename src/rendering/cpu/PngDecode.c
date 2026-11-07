/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Decodificador de PNG rapido (Wuffs) com fallback para o stb_image do raylib. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ZLIB
#define WUFFS_CONFIG__MODULE__PNG
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"
#endif
#include "wuffs-unsupported-snapshot.c"

#include "PngDecode.h"

#define PNG_MAX_DIM 16384

static uint8_t* ReadAll(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if(f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(n <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t* buf = (uint8_t*)malloc((size_t)n);
    if(buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

// true = ok. Qualquer erro devolve false e libera tudo.
static bool Wuffs_Decode(const char* path, Image* out) {
    size_t fileLen = 0;
    uint8_t* file = ReadAll(path, &fileLen);
    if(file == NULL)
        return false;

    bool ok = false;
    uint8_t* pixels = NULL;
    uint8_t* work = NULL;
    wuffs_png__decoder* dec = wuffs_png__decoder__alloc();
    if(dec == NULL)
        goto done;

    wuffs_base__io_buffer src = wuffs_base__make_io_buffer(wuffs_base__make_slice_u8(file, fileLen), wuffs_base__make_io_buffer_meta(fileLen, 0, 0, true));

    wuffs_base__image_config ic;
    wuffs_base__status st = wuffs_png__decoder__decode_image_config(dec, &ic, &src);
    if(!wuffs_base__status__is_ok(&st))
        goto done;

    uint32_t w = wuffs_base__pixel_config__width(&ic.pixcfg);
    uint32_t h = wuffs_base__pixel_config__height(&ic.pixcfg);
    if(w == 0 || h == 0 || w > PNG_MAX_DIM || h > PNG_MAX_DIM)
        goto done;

    wuffs_base__pixel_config__set(&ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);
    size_t pixLen = (size_t)w * h * 4;
    pixels = (uint8_t*)malloc(pixLen);
    uint64_t workLen = wuffs_png__decoder__workbuf_len(dec).max_incl;
    work = workLen > 0 ? (uint8_t*)malloc((size_t)workLen) : NULL;
    if(pixels == NULL || (workLen > 0 && work == NULL))
        goto done;

    wuffs_base__pixel_buffer pb;
    st = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, wuffs_base__make_slice_u8(pixels, pixLen));
    if(!wuffs_base__status__is_ok(&st))
        goto done;
    st = wuffs_png__decoder__decode_frame(dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, wuffs_base__make_slice_u8(work, (size_t)workLen), NULL);
    if(!wuffs_base__status__is_ok(&st))
        goto done;

    out->data = pixels;
    out->width = (int)w;
    out->height = (int)h;
    out->mipmaps = 1;
    out->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    pixels = NULL;
    ok = true;
done:
    free(pixels);
    free(work);
    free(dec);
    free(file);
    return ok;
}

Image PngDecode_Load(const char* path) {
    Image img = {0};
    if(path != NULL) {
        size_t n = strlen(path);
        if(n > 4 && (strcmp(path + n - 4, ".png") == 0 || strcmp(path + n - 4, ".PNG") == 0) && Wuffs_Decode(path, &img))
            return img;
    }
    return LoadImage(path);
}
