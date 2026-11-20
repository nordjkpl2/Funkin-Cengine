/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetSparrow.c - Loader de atlas Sparrow/Starling TextureAtlas XML para RayAnimationHandler.
 */

#include "logic/animations/AnimSetSparrow.h"
#include "logic/animations/AnimSetParseUtil.h"
#include "yxml.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Sparrow / Starling TextureAtlas XML                                       */
/* ------------------------------------------------------------------------- */

typedef struct AtlasFrame {
    char prefix[XML_MAX_PREFIX + 1u];
    uint32_t frameIndex;
    uint32_t sourceOrder;
    Frame frame;
} AtlasFrame;

typedef struct AtlasParser {
    AtlasFrame *frames;
    size_t count;
    size_t capacity;
    uint32_t order;

    unsigned char tagStack[XML_MAX_XML_DEPTH];
    size_t depth;

    bool inSubTexture;
    bool hasName;
    bool hasX;
    bool hasY;
    bool hasWidth;
    bool hasHeight;
    bool rotated;

    char name[XML_MAX_FRAME_NAME + 1u];
    int32_t x, y, width, height, frameX, frameY;

    int currentAttribute;
    char attrValue[XML_ATTR_VALUE_SIZE];
    size_t attrLength;
} AtlasParser;

enum {
    ATLAS_TAG_OTHER = 0,
    ATLAS_TAG_SUBTEXTURE = 1
};

enum {
    ATLAS_ATTR_NONE = 0,
    ATLAS_ATTR_NAME,
    ATLAS_ATTR_X,
    ATLAS_ATTR_Y,
    ATLAS_ATTR_WIDTH,
    ATLAS_ATTR_HEIGHT,
    ATLAS_ATTR_FRAMEX,
    ATLAS_ATTR_FRAMEY,
    ATLAS_ATTR_ROTATED
};

static bool Atlas_PushFrame(AtlasParser *parser)
{
    if (!parser->hasName || !parser->hasX || !parser->hasY ||
        !parser->hasWidth || !parser->hasHeight ||
        parser->width <= 0 || parser->height <= 0 ||
        parser->x < 0 || parser->y < 0 || parser->rotated)
        return false;

    if (parser->count >= XML_MAX_FRAMES) return false;

    if (parser->capacity == parser->count) {
        size_t newCapacity = parser->capacity ? parser->capacity * 2u : 128u;
        if (newCapacity > XML_MAX_FRAMES) newCapacity = XML_MAX_FRAMES;

        AtlasFrame *newFrames = realloc(parser->frames, newCapacity * sizeof(*newFrames));
        if (!newFrames) return false;

        parser->frames = newFrames;
        parser->capacity = newCapacity;
    }

    size_t nameLength = strlen(parser->name);
    size_t prefixLength = nameLength;
    while (prefixLength > 0 && isdigit((unsigned char)parser->name[prefixLength - 1u]))
        --prefixLength;

    uint32_t frameIndex = parser->order;
    if (prefixLength < nameLength) {
        if (prefixLength == 0) return false;

        errno = 0;
        char *end = NULL;
        unsigned long index = strtoul(parser->name + prefixLength, &end, 10);
        if (errno || end == parser->name + prefixLength || *end != '\0' || index > UINT32_MAX)
            return false;
        frameIndex = (uint32_t)index;
    }

    if (prefixLength == 0 || prefixLength > XML_MAX_PREFIX)
        return false;

    AtlasFrame *frame = &parser->frames[parser->count++];
    memcpy(frame->prefix, parser->name, prefixLength);
    frame->prefix[prefixLength] = '\0';
    frame->frameIndex = frameIndex;
    frame->sourceOrder = parser->order++;

    frame->frame.x = parser->x;
    frame->frame.y = parser->y;
    frame->frame.w = parser->width;
    frame->frame.h = parser->height;
    frame->frame.fx = parser->frameX;
    frame->frame.fy = parser->frameY;

    return true;
}

static int AtlasFrame_Compare(const void *left, const void *right)
{
    const AtlasFrame *a = left;
    const AtlasFrame *b = right;

    int names = strcmp(a->prefix, b->prefix);
    if (names) return names;

    if (a->frameIndex < b->frameIndex) return -1;
    if (a->frameIndex > b->frameIndex) return 1;

    if (a->sourceOrder < b->sourceOrder) return -1;
    if (a->sourceOrder > b->sourceOrder) return 1;
    return 0;
}

static void Atlas_ResetSubTexture(AtlasParser *parser)
{
    parser->hasName = false;
    parser->hasX = false;
    parser->hasY = false;
    parser->hasWidth = false;
    parser->hasHeight = false;
    parser->rotated = false;
    parser->name[0] = '\0';
    parser->x = parser->y = 0;
    parser->width = parser->height = 0;
    parser->frameX = parser->frameY = 0;
}

static bool Atlas_StoreAttribute(AtlasParser *parser)
{
    int32_t number;
    bool boolean;

    switch (parser->currentAttribute) {
        case ATLAS_ATTR_NAME: {
            size_t length = strlen(parser->attrValue);
            if (length == 0 || length > XML_MAX_FRAME_NAME) return false;
            memcpy(parser->name, parser->attrValue, length + 1u);
            parser->hasName = true;
        } break;

        case ATLAS_ATTR_X:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->x = number;
            parser->hasX = true;
            break;

        case ATLAS_ATTR_Y:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->y = number;
            parser->hasY = true;
            break;

        case ATLAS_ATTR_WIDTH:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->width = number;
            parser->hasWidth = true;
            break;

        case ATLAS_ATTR_HEIGHT:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->height = number;
            parser->hasHeight = true;
            break;

        case ATLAS_ATTR_FRAMEX:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->frameX = number;
            break;

        case ATLAS_ATTR_FRAMEY:
            if (!AnimParse_Int32(parser->attrValue, &number)) return false;
            parser->frameY = number;
            break;

        case ATLAS_ATTR_ROTATED:
            if (!AnimParse_Bool(parser->attrValue, &boolean)) return false;
            parser->rotated = boolean;
            break;

        default:
            break;
    }

    return true;
}

static bool Atlas_XmlToken(yxml_t *xml, yxml_ret_t token, void *user)
{
    AtlasParser *parser = user;

    switch (token) {
        case YXML_ELEMSTART: {
            if (parser->depth >= XML_MAX_XML_DEPTH) return false;
            unsigned char tag = !strcmp(xml->elem, "SubTexture")
                ? ATLAS_TAG_SUBTEXTURE : ATLAS_TAG_OTHER;
            parser->tagStack[parser->depth++] = tag;

            if (tag == ATLAS_TAG_SUBTEXTURE) {
                parser->inSubTexture = true;
                Atlas_ResetSubTexture(parser);
            }
        } break;

        case YXML_ATTRSTART:
            parser->currentAttribute = ATLAS_ATTR_NONE;
            parser->attrLength = 0;
            parser->attrValue[0] = '\0';

            if (parser->depth && parser->tagStack[parser->depth - 1u] == ATLAS_TAG_SUBTEXTURE) {
                if (!strcmp(xml->attr, "name")) parser->currentAttribute = ATLAS_ATTR_NAME;
                else if (!strcmp(xml->attr, "x")) parser->currentAttribute = ATLAS_ATTR_X;
                else if (!strcmp(xml->attr, "y")) parser->currentAttribute = ATLAS_ATTR_Y;
                else if (!strcmp(xml->attr, "width")) parser->currentAttribute = ATLAS_ATTR_WIDTH;
                else if (!strcmp(xml->attr, "height")) parser->currentAttribute = ATLAS_ATTR_HEIGHT;
                else if (!strcmp(xml->attr, "frameX")) parser->currentAttribute = ATLAS_ATTR_FRAMEX;
                else if (!strcmp(xml->attr, "frameY")) parser->currentAttribute = ATLAS_ATTR_FRAMEY;
                else if (!strcmp(xml->attr, "rotated")) parser->currentAttribute = ATLAS_ATTR_ROTATED;
            }
            break;

        case YXML_ATTRVAL:
            if (parser->currentAttribute != ATLAS_ATTR_NONE &&
                !AnimParse_AppendAttr(parser->attrValue, sizeof(parser->attrValue),
                    &parser->attrLength, xml->data))
                return false;
            break;

        case YXML_ATTREND:
            if (parser->currentAttribute != ATLAS_ATTR_NONE && !Atlas_StoreAttribute(parser))
                return false;
            parser->currentAttribute = ATLAS_ATTR_NONE;
            break;

        case YXML_ELEMEND:
            if (!parser->depth) return false;
            if (parser->tagStack[--parser->depth] == ATLAS_TAG_SUBTEXTURE) {
                if (!Atlas_PushFrame(parser)) return false;
                parser->inSubTexture = false;
            }
            break;

        default:
            break;
    }

    return true;
}


bool Atlas_LoadSparrowXML(const char *path, Texture2D image, RayAnimationHandler *out)
{
    if (!path || !out) return false;

    AtlasParser parser = {0};
    if (!AnimParse_YxmlFile(path, Atlas_XmlToken, &parser)) {
        free(parser.frames);
        return false;
    }

    if (parser.count == 0) {
        free(parser.frames);
        fprintf(stderr, "[AnimationSets] no SubTexture frames found in %s\n", path);
        return false;
    }

    /* Confere se os retângulos do atlas cabem na textura que foi fornecida. */
    for (size_t i = 0; i < parser.count; ++i) {
        const Frame *frame = &parser.frames[i].frame;
        if (frame->x < 0 || frame->y < 0 ||
            (int64_t)frame->x + frame->w > image.width ||
            (int64_t)frame->y + frame->h > image.height) {
            free(parser.frames);
            fprintf(stderr, "[AnimationSets] SubTexture outside texture bounds in %s\n", path);
            return false;
        }
    }

    qsort(parser.frames, parser.count, sizeof(*parser.frames), AtlasFrame_Compare);

    size_t animationCount = 0;
    for (size_t i = 0; i < parser.count; ) {
        ++animationCount;
        const char *prefix = parser.frames[i].prefix;
        do { ++i; } while (i < parser.count && !strcmp(parser.frames[i].prefix, prefix));
    }

    if (animationCount == 0 || animationCount > ANIMSET_MAX_ANIMATIONS ||
        animationCount > INT_MAX) {
        free(parser.frames);
        return false;
    }

    RayAnimation *animations = calloc(animationCount, sizeof(*animations));
    Frame *frameArena = malloc(parser.count * sizeof(*frameArena));
    if (!animations || !frameArena) {
        free(animations);
        free(frameArena);
        free(parser.frames);
        return false;
    }

    Frame *arena = frameArena;
    size_t animationIndex = 0;

    for (size_t i = 0; i < parser.count; ) {
        size_t start = i;
        const char *prefix = parser.frames[i].prefix;
        do { ++i; } while (i < parser.count && !strcmp(parser.frames[i].prefix, prefix));

        size_t count = i - start;
        RayAnimation *animation = &animations[animationIndex++];

        if (strlen(prefix) > sizeof(animation->name) - 1u || count > INT_MAX) {
            free(animations);
            free(frameArena);
            free(parser.frames);
            return false;
        }

        memcpy(animation->name, prefix, strlen(prefix) + 1u);
        animation->frameCount = (int)count;
        animation->frames = arena;
        animation->animationOffset = (Vector2){0.0f, 0.0f};
        animation->fps = 24.0f; /* Sparrow atlas não armazena FPS. */
        animation->frameTime = 1.0f / 24.0f;
        animation->looped = 0;

        for (size_t j = start; j < i; ++j)
            *arena++ = parser.frames[j].frame;
    }

    free(parser.frames);

    *out = (RayAnimationHandler){
        .texture = image,
        .animations = animations,
        .frameArena = frameArena,
        .animationCount = (int)animationCount
    };

    return true;
}

