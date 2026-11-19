/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetBinary.c - Loader do formato binario .animset.
 */

#include "std.h"
#include "logic/animations/AnimSetBinary.h"
#include "logic/animations/AnimSetParseUtil.h"
#include "Log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Binary .animset loader (keeps the original file format)                    */
/* ------------------------------------------------------------------------- */

bool Binary_LoadAnimSet(const char *path, Texture2D image, RayAnimationHandler *out)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "[AnimationSets] cannot open animation set: %s\n", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long fileSizeLong = ftell(file);
    if (fileSizeLong < (long)sizeof(int32_t) || fileSizeLong > 128L * 1024L * 1024L ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    size_t fileSize = (size_t)fileSizeLong;
    uint8_t *buffer = malloc(fileSize);
    if (!buffer) {
        fclose(file);
        return false;
    }

    bool readOK = fread(buffer, 1, fileSize, file) == fileSize;
    fclose(file);
    if (!readOK) {
        free(buffer);
        return false;
    }

    uint8_t *ptr = buffer;
    uint8_t *end = buffer + fileSize;
    int32_t animationCount = 0;

    if ((size_t)(end - ptr) < sizeof(animationCount)) goto fail;
    memcpy(&animationCount, ptr, sizeof(animationCount));
    ptr += sizeof(animationCount);

    if (animationCount < 1 || animationCount > ANIMSET_MAX_ANIMATIONS) goto fail;

    uint8_t *scan = ptr;
    size_t totalFrames = 0;

    for (int32_t i = 0; i < animationCount; ++i) {
        int32_t nameLength;
        int32_t dataCount;

        if ((size_t)(end - scan) < sizeof(nameLength)) goto fail;
        memcpy(&nameLength, scan, sizeof(nameLength));
        scan += sizeof(nameLength);

        if (nameLength < 1 || nameLength > (int32_t)XML_MAX_ANIM_NAME ||
            (size_t)(end - scan) < (size_t)nameLength + sizeof(dataCount))
            goto fail;

        scan += nameLength;
        memcpy(&dataCount, scan, sizeof(dataCount));
        scan += sizeof(dataCount);

        if (dataCount < 6 || dataCount % 6 != 0) goto fail;

        size_t frameCount = (size_t)(dataCount / 6);
        if (frameCount > XML_MAX_FRAMES ||
            frameCount > (size_t)(end - scan) / sizeof(Frame))
            goto fail;

        if (totalFrames > XML_MAX_FRAMES - frameCount) goto fail;
        totalFrames += frameCount;
        scan += frameCount * sizeof(Frame);
    }

    if (scan != end) goto fail;

    RayAnimation *animations = calloc((size_t)animationCount, sizeof(*animations));
    Frame *frameArena = malloc(totalFrames * sizeof(*frameArena));
    if (!animations || !frameArena) {
        free(animations);
        free(frameArena);
        goto fail;
    }

    Frame *arena = frameArena;

    for (int32_t i = 0; i < animationCount; ++i) {
        int32_t nameLength;
        int32_t dataCount;

        memcpy(&nameLength, ptr, sizeof(nameLength));
        ptr += sizeof(nameLength);

        RayAnimation *animation = &animations[i];
        memcpy(animation->name, ptr, (size_t)nameLength);
        animation->name[nameLength] = '\0';
        ptr += nameLength;

        memcpy(&dataCount, ptr, sizeof(dataCount));
        ptr += sizeof(dataCount);

        size_t frameCount = (size_t)(dataCount / 6);
        animation->frameCount = (int)frameCount;
        animation->frames = arena;
        animation->animationOffset = (Vector2){0.0f, 0.0f};
        animation->fps = 1.0f;
        animation->frameTime = 1.0f;
        animation->looped = 0;

        memcpy(arena, ptr, frameCount * sizeof(Frame));
        ptr += frameCount * sizeof(Frame);

        for (size_t j = 0; j < frameCount; ++j) {
            if (arena[j].w <= 0 || arena[j].h <= 0) {
                free(animations);
                free(frameArena);
                goto fail;
            }
        }

        arena += frameCount;
    }

    free(buffer);

    *out = (RayAnimationHandler){
        .texture = image,
        .animations = animations,
        .frameArena = frameArena,
        .animationCount = animationCount
    };
    return true;

fail:
    free(buffer);
    fprintf(stderr, "[AnimationSets] invalid .animset data: %s\n", path);
    return false;
}

