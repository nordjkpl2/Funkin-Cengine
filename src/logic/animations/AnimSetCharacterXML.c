/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetCharacterXML.c - Parser de XML de personagem estilo Codename (<character><anim/></character>).
 */

#include "logic/animations/AnimSetCharacterXML.h"
#include "logic/animations/AnimSetParseUtil.h"
#include "logic/animations/AnimSetSparrow.h"
#include "yxml.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Codename-style character XML: <character><anim ... /></character>        */
/* ------------------------------------------------------------------------- */

typedef struct CharacterAnimDef {
    char name[XML_MAX_ANIM_NAME + 1u];
    char prefix[XML_MAX_PREFIX + 1u];
    char indices[XML_ATTR_VALUE_SIZE];
    float fps;
    float offsetX;
    float offsetY;
    bool looped;
    bool hasName;
    bool hasPrefix;
} CharacterAnimDef;

typedef struct CharacterXMLParser {
    RayCharacterXMLData data;

    CharacterAnimDef *anims;
    size_t animCount;
    size_t animCapacity;
    CharacterAnimDef currentAnim;

    unsigned char tagStack[XML_MAX_XML_DEPTH];
    size_t depth;

    int currentAttribute;
    char attrValue[XML_ATTR_VALUE_SIZE];
    size_t attrLength;
    bool hasCharacterRoot;
} CharacterXMLParser;

enum {
    CHAR_TAG_OTHER = 0,
    CHAR_TAG_CHARACTER = 1,
    CHAR_TAG_ANIM = 2
};

enum {
    CHAR_ATTR_NONE = 0,
    CHAR_ATTR_SPRITE,
    CHAR_ATTR_X,
    CHAR_ATTR_Y,
    CHAR_ATTR_SCALE,
    CHAR_ATTR_CAMX,
    CHAR_ATTR_CAMY,
    CHAR_ATTR_HOLDTIME,
    CHAR_ATTR_DEFFPS,
    CHAR_ATTR_ISPLAYER,
    CHAR_ATTR_FLIPX,
    CHAR_ATTR_ANTIALIASING,
    CHAR_ATTR_COLOR,
    CHAR_ATTR_NAME,
    CHAR_ATTR_ANIM,
    CHAR_ATTR_FPS,
    CHAR_ATTR_LOOP,
    CHAR_ATTR_INDICES,
    CHAR_ATTR_ANIM_X,
    CHAR_ATTR_ANIM_Y
};

static void CharacterXML_Init(CharacterXMLParser *parser)
{
    memset(parser, 0, sizeof(*parser));
    parser->data.scale = 1.0f;
    parser->data.holdTime = 4.0f;
    parser->data.defaultFPS = 24.0f;
    parser->data.color = WHITE;
    parser->data.antialiasing = true;
}

static bool CharacterXML_PushAnim(CharacterXMLParser *parser)
{
    if (!parser->currentAnim.hasName || parser->animCount >= ANIMSET_MAX_ANIMATIONS)
        return false;

    if (!parser->currentAnim.hasPrefix) {
        memcpy(parser->currentAnim.prefix, parser->currentAnim.name,
            strlen(parser->currentAnim.name) + 1u);
    }

    for (size_t i = 0; i < parser->animCount; ++i) {
        if (!strcmp(parser->anims[i].name, parser->currentAnim.name))
            return false;
    }

    if (parser->animCount == parser->animCapacity) {
        size_t newCapacity = parser->animCapacity ? parser->animCapacity * 2u : 16u;
        if (newCapacity > ANIMSET_MAX_ANIMATIONS)
            newCapacity = ANIMSET_MAX_ANIMATIONS;

        CharacterAnimDef *newAnims = realloc(parser->anims, newCapacity * sizeof(*newAnims));
        if (!newAnims) return false;

        parser->anims = newAnims;
        parser->animCapacity = newCapacity;
    }

    parser->anims[parser->animCount++] = parser->currentAnim;
    return true;
}

static bool CharacterXML_StoreAttribute(CharacterXMLParser *parser)
{
    bool boolean;
    RayCharacterXMLData *data = &parser->data;

    switch (parser->currentAttribute) {
        case CHAR_ATTR_SPRITE: {
            size_t length = strlen(parser->attrValue);
            if (length >= sizeof(data->sprite)) return false;
            memcpy(data->sprite, parser->attrValue, length + 1u);
        } break;

        case CHAR_ATTR_X:
            if (!AnimParse_Float(parser->attrValue, &data->x)) return false;
            data->hasX = true;
            break;
        case CHAR_ATTR_Y:
            if (!AnimParse_Float(parser->attrValue, &data->y)) return false;
            data->hasY = true;
            break;
        case CHAR_ATTR_SCALE:
            if (!AnimParse_Float(parser->attrValue, &data->scale) || data->scale <= 0.0f) return false;
            data->hasScale = true;
            break;
        case CHAR_ATTR_CAMX:
            if (!AnimParse_Float(parser->attrValue, &data->cameraX)) return false;
            data->hasCameraX = true;
            break;
        case CHAR_ATTR_CAMY:
            if (!AnimParse_Float(parser->attrValue, &data->cameraY)) return false;
            data->hasCameraY = true;
            break;
        case CHAR_ATTR_HOLDTIME:
            if (!AnimParse_Float(parser->attrValue, &data->holdTime) || data->holdTime < 0.0f) return false;
            break;
        case CHAR_ATTR_DEFFPS:
            if (!AnimParse_Float(parser->attrValue, &data->defaultFPS) ||
                data->defaultFPS <= 0.0f || data->defaultFPS > 1000.0f) return false;
            break;
        case CHAR_ATTR_ISPLAYER:
            if (!AnimParse_Bool(parser->attrValue, &boolean)) return false;
            data->isPlayer = boolean;
            break;
        case CHAR_ATTR_FLIPX:
            if (!AnimParse_Bool(parser->attrValue, &boolean)) return false;
            data->flipX = boolean;
            break;
        case CHAR_ATTR_ANTIALIASING:
            if (!AnimParse_Bool(parser->attrValue, &boolean)) return false;
            data->antialiasing = boolean;
            break;
        case CHAR_ATTR_COLOR:
            if (!AnimParse_Color(parser->attrValue, &data->color)) return false;
            data->hasColor = true;
            break;

        case CHAR_ATTR_ANIM_X:
            if (!AnimParse_Float(parser->attrValue, &parser->currentAnim.offsetX)) return false;
            break;
        case CHAR_ATTR_ANIM_Y:
            if (!AnimParse_Float(parser->attrValue, &parser->currentAnim.offsetY)) return false;
            break;

        case CHAR_ATTR_NAME: {
            size_t length = strlen(parser->attrValue);
            if (length == 0 || length > XML_MAX_ANIM_NAME) return false;
            memcpy(parser->currentAnim.name, parser->attrValue, length + 1u);
            parser->currentAnim.hasName = true;
        } break;
        case CHAR_ATTR_ANIM: {
            size_t length = strlen(parser->attrValue);
            if (length == 0 || length > XML_MAX_PREFIX) return false;
            memcpy(parser->currentAnim.prefix, parser->attrValue, length + 1u);
            parser->currentAnim.hasPrefix = true;
        } break;
        case CHAR_ATTR_FPS:
            if (!AnimParse_Float(parser->attrValue, &parser->currentAnim.fps) ||
                parser->currentAnim.fps <= 0.0f || parser->currentAnim.fps > 1000.0f)
                return false;
            break;
        case CHAR_ATTR_LOOP:
            if (!AnimParse_Bool(parser->attrValue, &parser->currentAnim.looped)) return false;
            break;
        case CHAR_ATTR_INDICES: {
            size_t length = strlen(parser->attrValue);
            if (length >= sizeof(parser->currentAnim.indices)) return false;
            memcpy(parser->currentAnim.indices, parser->attrValue, length + 1u);
        } break;

        default:
            break;
    }

    return true;
}

static bool CharacterXML_XmlToken(yxml_t *xml, yxml_ret_t token, void *user)
{
    CharacterXMLParser *parser = user;

    switch (token) {
        case YXML_ELEMSTART: {
            if (parser->depth >= XML_MAX_XML_DEPTH) return false;

            unsigned char tag = CHAR_TAG_OTHER;
            if (!strcmp(xml->elem, "character")) {
                tag = CHAR_TAG_CHARACTER;
                parser->hasCharacterRoot = true;
            } else if (!strcmp(xml->elem, "anim")) {
                tag = CHAR_TAG_ANIM;
                memset(&parser->currentAnim, 0, sizeof(parser->currentAnim));
            }

            parser->tagStack[parser->depth++] = tag;
        } break;

        case YXML_ATTRSTART: {
            parser->currentAttribute = CHAR_ATTR_NONE;
            parser->attrLength = 0;
            parser->attrValue[0] = '\0';

            unsigned char tag = parser->depth ? parser->tagStack[parser->depth - 1u] : CHAR_TAG_OTHER;

            if (tag == CHAR_TAG_CHARACTER) {
                if (!strcmp(xml->attr, "sprite")) parser->currentAttribute = CHAR_ATTR_SPRITE;
                else if (!strcmp(xml->attr, "x")) parser->currentAttribute = CHAR_ATTR_X;
                else if (!strcmp(xml->attr, "y")) parser->currentAttribute = CHAR_ATTR_Y;
                else if (!strcmp(xml->attr, "scale")) parser->currentAttribute = CHAR_ATTR_SCALE;
                else if (!strcmp(xml->attr, "camx")) parser->currentAttribute = CHAR_ATTR_CAMX;
                else if (!strcmp(xml->attr, "camy")) parser->currentAttribute = CHAR_ATTR_CAMY;
                else if (!strcmp(xml->attr, "holdTime")) parser->currentAttribute = CHAR_ATTR_HOLDTIME;
                else if (!strcmp(xml->attr, "defFps")) parser->currentAttribute = CHAR_ATTR_DEFFPS;
                else if (!strcmp(xml->attr, "isPlayer")) parser->currentAttribute = CHAR_ATTR_ISPLAYER;
                else if (!strcmp(xml->attr, "flipX")) parser->currentAttribute = CHAR_ATTR_FLIPX;
                else if (!strcmp(xml->attr, "antialiasing")) parser->currentAttribute = CHAR_ATTR_ANTIALIASING;
                else if (!strcmp(xml->attr, "color")) parser->currentAttribute = CHAR_ATTR_COLOR;
            } else if (tag == CHAR_TAG_ANIM) {
                if (!strcmp(xml->attr, "name")) parser->currentAttribute = CHAR_ATTR_NAME;
                else if (!strcmp(xml->attr, "anim")) parser->currentAttribute = CHAR_ATTR_ANIM;
                else if (!strcmp(xml->attr, "fps")) parser->currentAttribute = CHAR_ATTR_FPS;
                else if (!strcmp(xml->attr, "loop")) parser->currentAttribute = CHAR_ATTR_LOOP;
                else if (!strcmp(xml->attr, "indices")) parser->currentAttribute = CHAR_ATTR_INDICES;
                else if (!strcmp(xml->attr, "x")) parser->currentAttribute = CHAR_ATTR_ANIM_X;
                else if (!strcmp(xml->attr, "y")) parser->currentAttribute = CHAR_ATTR_ANIM_Y;
            }
        } break;

        case YXML_ATTRVAL:
            if (parser->currentAttribute != CHAR_ATTR_NONE &&
                !AnimParse_AppendAttr(parser->attrValue, sizeof(parser->attrValue),
                    &parser->attrLength, xml->data))
                return false;
            break;

        case YXML_ATTREND:
            if (parser->currentAttribute != CHAR_ATTR_NONE &&
                !CharacterXML_StoreAttribute(parser))
                return false;
            parser->currentAttribute = CHAR_ATTR_NONE;
            break;

        case YXML_ELEMEND:
            if (!parser->depth) return false;
            if (parser->tagStack[--parser->depth] == CHAR_TAG_ANIM) {
                if (!CharacterXML_PushAnim(parser)) return false;
            }
            break;

        default:
            break;
    }

    return true;
}

static const RayAnimation *FindPrefixAnimation(const RayAnimationHandler *atlas,
                                               const char *prefix)
{
    for (int i = 0; i < atlas->animationCount; ++i) {
        if (!strcmp(atlas->animations[i].name, prefix))
            return &atlas->animations[i];
    }
    return NULL;
}

/* Walks indices like "0, 2, 4..7". With dst == NULL it only counts frames. */
static bool Animation_WalkIndices(const char *expression,
                                  const Frame *source,
                                  size_t sourceCount,
                                  Frame *dst,
                                  size_t dstCapacity,
                                  size_t *outCount)
{
    size_t written = 0;

    if (!expression || !expression[0]) {
        if (dst && sourceCount > dstCapacity) return false;
        if (dst) memcpy(dst, source, sourceCount * sizeof(*dst));
        *outCount = sourceCount;
        return true;
    }

    const char *cursor = expression;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == ';')
            ++cursor;
        if (!*cursor) break;

        errno = 0;
        char *end = NULL;
        long first = strtol(cursor, &end, 10);
        if (errno || end == cursor || first < 0 || (size_t)first >= sourceCount)
            return false;
        cursor = end;

        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        long last = first;

        if (cursor[0] == '.' && cursor[1] == '.') {
            cursor += 2;
            while (*cursor == ' ' || *cursor == '\t') ++cursor;
            errno = 0;
            last = strtol(cursor, &end, 10);
            if (errno || end == cursor || last < 0 || (size_t)last >= sourceCount)
                return false;
            cursor = end;
        }

        long step = first <= last ? 1 : -1;
        for (long index = first;; index += step) {
            if (written >= XML_MAX_FRAMES || (dst && written >= dstCapacity))
                return false;
            if (dst) dst[written] = source[index];
            ++written;
            if (index == last) break;
        }

        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        if (*cursor && *cursor != ',' && *cursor != ';')
            return false;
    }

    if (written == 0) return false;
    *outCount = written;
    return true;
}

bool AnimationSet_LoadCharacterXML(const char *characterXMLPath,
                                   const char *atlasXMLPath,
                                   Texture2D image,
                                   RayCharacterXMLData *out)
{
    if (!characterXMLPath || !atlasXMLPath || !out)
        return false;

    CharacterXMLParser character;
    CharacterXML_Init(&character);

    if (!AnimParse_YxmlFile(characterXMLPath, CharacterXML_XmlToken, &character)) {
        free(character.anims);
        return false;
    }

    if (!character.hasCharacterRoot || character.animCount == 0) {
        free(character.anims);
        fprintf(stderr, "[AnimationSets] character XML has no <character>/<anim>: %s\n",
            characterXMLPath);
        return false;
    }

    RayAnimationHandler atlas = {0};
    if (!Atlas_LoadSparrowXML(atlasXMLPath, image, &atlas)) {
        free(character.anims);
        return false;
    }

    size_t totalFrames = 0;
    for (size_t i = 0; i < character.animCount; ++i) {
        CharacterAnimDef *def = &character.anims[i];
        const RayAnimation *source = FindPrefixAnimation(&atlas, def->prefix);

        if (!source) {
            fprintf(stderr, "[AnimationSets] atlas prefix '%s' not found (character: %s)\n",
                def->prefix, characterXMLPath);
            AnimationSet_FreeArrays(&atlas);
            free(character.anims);
            return false;
        }

        size_t selected = 0;
        if (!Animation_WalkIndices(def->indices, source->frames,
                (size_t)source->frameCount, NULL, 0, &selected) ||
            selected > XML_MAX_FRAMES - totalFrames) {
            AnimationSet_FreeArrays(&atlas);
            free(character.anims);
            return false;
        }
        totalFrames += selected;

        if (def->fps <= 0.0f)
            def->fps = character.data.defaultFPS;
    }

    RayAnimation *animations = calloc(character.animCount, sizeof(*animations));
    Frame *frameArena = malloc(totalFrames * sizeof(*frameArena));

    if (!animations || !frameArena) {
        free(animations);
        free(frameArena);
        AnimationSet_FreeArrays(&atlas);
        free(character.anims);
        return false;
    }

    Frame *arena = frameArena;

    for (size_t i = 0; i < character.animCount; ++i) {
        const CharacterAnimDef *def = &character.anims[i];
        const RayAnimation *source = FindPrefixAnimation(&atlas, def->prefix);
        RayAnimation *animation = &animations[i];

        size_t selected = 0;
        if (!Animation_WalkIndices(def->indices, source->frames,
                (size_t)source->frameCount, arena, totalFrames - (size_t)(arena - frameArena),
                &selected)) {
            free(animations);
            free(frameArena);
            AnimationSet_FreeArrays(&atlas);
            free(character.anims);
            return false;
        }

        memcpy(animation->name, def->name, strlen(def->name) + 1u);
        animation->frames = arena;
        animation->frameCount = (int)selected;
        animation->fps = def->fps;
        animation->frameTime = 1.0f / def->fps;
        animation->looped = def->looped;
        animation->animationOffset = (Vector2){def->offsetX, def->offsetY};

        arena += selected;
    }

    AnimationSet_FreeArrays(&atlas);
    free(character.anims);

    character.data.animations = (RayAnimationHandler){
        .texture = image,
        .animations = animations,
        .frameArena = frameArena,
        .animationCount = (int)character.animCount
    };

    *out = character.data;
    return true;
}
