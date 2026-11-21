/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteAtlas.c - atlas das cores de lane (ver PlayStateNoteAtlas.h).
 *
 * Layout: 4 colunas lado a lado, cada uma com a skin inteira (w x h) recolorida na cor
 * da lane. Os frames de cada variante sao copias dos frames base com x deslocado.
 *
 * A recolorizacao segue o shader de matiz de PlayStateNoteSpawn.c: troca o matiz pelo da
 * cor alvo e multiplica saturacao e brilho pelos da cor alvo; alpha e preservado.
 */

#include "scenes/gameplay/PlayStateNoteAtlas.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_LANES 4
#define ATLAS_MAX_SIDE 8192   /* limite conservador de textura (GL 3.3 garante 8192 em qualquer GPU) */

typedef struct {
    Frame* arena;
    RayAnimation* animations;
} LaneStorage;

static Texture2D atlasTexture;
static RayAnimationHandler laneSets[ATLAS_LANES];
static LaneStorage laneStorage[ATLAS_LANES];
static bool atlasReady;

static bool IsWhite(Color c) {
    return c.r == 255 && c.g == 255 && c.b == 255;
}

/* HSV -> RGB (0..1), mesma formula do hsv2rgb do shader. h em 0..1. */
static void Hsv2Rgb(float h, float s, float v, float* r, float* g, float* b) {
    const float hh = (h - floorf(h)) * 6.0f;
    const int i = (int)hh % 6;
    const float f = hh - floorf(hh);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    switch (i) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static unsigned char ToByte(float x) {
    x = fminf(fmaxf(x, 0.0f), 1.0f);
    return (unsigned char)(x * 255.0f + 0.5f);
}

/* Recolore uma linha de `count` pixels RGBA8 para a cor alvo. */
static void RecolorRow(unsigned char* dst, const unsigned char* src, int count, const Vector3 target) {
    const float th = target.x / 360.0f;
    for (int x = 0; x < count; x++, src += 4, dst += 4) {
        const float r = src[0] / 255.0f, g = src[1] / 255.0f, b = src[2] / 255.0f;
        const float max = fmaxf(r, fmaxf(g, b));
        const float min = fminf(r, fminf(g, b));
        const float s = (max > 0.0f ? (max - min) / max : 0.0f) * target.y;
        const float v = max * target.z;
        float or_, og, ob;
        Hsv2Rgb(th, s, v, &or_, &og, &ob);
        dst[0] = ToByte(or_);
        dst[1] = ToByte(og);
        dst[2] = ToByte(ob);
        dst[3] = src[3];
    }
}

static void FreeLanes(void) {
    for (int lane = 0; lane < ATLAS_LANES; lane++) {
        free(laneStorage[lane].arena);
        free(laneStorage[lane].animations);
        laneStorage[lane] = (LaneStorage){0};
        laneSets[lane] = (RayAnimationHandler){0};
    }
}

void NoteAtlas_Free(void) {
    FreeLanes();
    if (atlasTexture.id != 0)
        UnloadTexture(atlasTexture);
    atlasTexture = (Texture2D){0};
    atlasReady = false;
}

/* Frames e animacoes da variante: copias com x deslocado pela coluna da lane. */
static bool BuildLaneSet(const RayAnimationHandler* base, int lane, int shiftX) {
    const int animCount = base->animationCount;
    size_t totalFrames = 0;
    for (int i = 0; i < animCount; i++) {
        if (base->animations[i].frameCount > 0)
            totalFrames += (size_t)base->animations[i].frameCount;
    }

    RayAnimation* anims = malloc(sizeof(RayAnimation) * (size_t)(animCount > 0 ? animCount : 1));
    Frame* arena = malloc(sizeof(Frame) * (totalFrames > 0 ? totalFrames : 1));
    if (anims == NULL || arena == NULL) {
        free(anims);
        free(arena);
        return false;
    }

    size_t offset = 0;
    for (int i = 0; i < animCount; i++) {
        const RayAnimation* src = &base->animations[i];
        anims[i] = *src;
        anims[i].frames = NULL;
        if (src->frames != NULL && src->frameCount > 0) {
            for (int f = 0; f < src->frameCount; f++) {
                Frame frame = src->frames[f];
                frame.x += shiftX;
                arena[offset + (size_t)f] = frame;
            }
            anims[i].frames = arena + offset;
            offset += (size_t)src->frameCount;
        }
    }

    laneStorage[lane].arena = arena;
    laneStorage[lane].animations = anims;
    laneSets[lane] = (RayAnimationHandler){
        .texture = atlasTexture,
        .animations = anims,
        .frameArena = NULL,
        .animationCount = animCount,
    };
    return true;
}

bool NoteAtlas_Build(const RayAnimationHandler* base, const Color tints[4]) {
    NoteAtlas_Free();

    if (base == NULL || base->texture.id == 0 || base->animations == NULL || base->animationCount <= 0)
        return false;

    /* Todas brancas: a skin original ja serve, nao ha atlas para gerar. */
    if (IsWhite(tints[0]) && IsWhite(tints[1]) && IsWhite(tints[2]) && IsWhite(tints[3]))
        return false;

    const int w = base->texture.width;
    const int h = base->texture.height;
    if (w <= 0 || h <= 0 || w * ATLAS_LANES > ATLAS_MAX_SIDE || h > ATLAS_MAX_SIDE)
        return false;

    Image src = LoadImageFromTexture(base->texture);
    if (src.data == NULL)
        return false;
    ImageFormat(&src, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Image atlas = GenImageColor(w * ATLAS_LANES, h, BLANK);
    if (atlas.data == NULL) {
        UnloadImage(src);
        return false;
    }
    ImageFormat(&atlas, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    const unsigned char* srcPx = (const unsigned char*)src.data;
    unsigned char* dstPx = (unsigned char*)atlas.data;
    const size_t srcRow = (size_t)w * 4;
    const size_t dstRow = (size_t)w * ATLAS_LANES * 4;

    for (int lane = 0; lane < ATLAS_LANES; lane++) {
        const Vector3 target = ColorToHSV(tints[lane]);
        for (int y = 0; y < h; y++) {
            unsigned char* out = dstPx + (size_t)y * dstRow + (size_t)lane * srcRow;
            const unsigned char* in = srcPx + (size_t)y * srcRow;
            if (IsWhite(tints[lane]))
                memcpy(out, in, srcRow);
            else
                RecolorRow(out, in, w, target);
        }
    }

    atlasTexture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    UnloadImage(src);
    if (atlasTexture.id == 0)
        return false;

    SetTextureFilter(atlasTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(atlasTexture, TEXTURE_WRAP_CLAMP);

    for (int lane = 0; lane < ATLAS_LANES; lane++) {
        if (!BuildLaneSet(base, lane, lane * w)) {
            NoteAtlas_Free();
            return false;
        }
    }

    atlasReady = true;
    return true;
}

const RayAnimationHandler* NoteAtlas_Lane(int lane) {
    if (!atlasReady || lane < 0 || lane >= ATLAS_LANES)
        return NULL;
    return &laneSets[lane];
}
