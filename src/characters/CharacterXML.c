/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterXML.c - Leitura de personagem Codename em XML (sprite, atlas e metadados).
 */

#include "characters/Character.h"
#include "characters/CharacterInternal.h"
#include "logic/animations/CharacterAnims.h"
#include "characters/CharacterXML.h"
#include "logic/animations/AnimationSets_XML.h"
#include "yxml.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Personagem em XML (Codename)                                              */
/* ------------------------------------------------------------------------- */

/* Le so o atributo sprite="..." da tag <character>; o parse completo fica
   em AnimationSet_LoadCharacterXML(). */
static bool Char_ReadXMLSprite(const char *path, char *sprite, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;

    yxml_t parser;
    unsigned char stack[CHAR_XML_STACK_SIZE];
    unsigned char input[CHAR_XML_READ_SIZE];
    yxml_init(&parser, stack, sizeof(stack));

    bool foundRoot = false;
    bool inCharacterTag = false;
    bool readingSprite = false;
    bool ok = true;
    size_t length = 0;
    size_t bytesRead;

    sprite[0] = '\0';

    while (ok && (bytesRead = fread(input, 1, sizeof(input), file)) != 0) {
        for (size_t i = 0; i < bytesRead && ok; ++i) {
            yxml_ret_t token = yxml_parse(&parser, input[i]);
            if (token < 0) {
                ok = false;
                break;
            }

            switch (token) {
                case YXML_ELEMSTART:
                    if (!strcmp(parser.elem, "character"))
                        foundRoot = inCharacterTag = true;
                    break;

                case YXML_ATTRSTART:
                    readingSprite = inCharacterTag && !strcmp(parser.attr, "sprite");
                    break;

                case YXML_ATTRVAL:
                    if (readingSprite) {
                        size_t part = strlen(parser.data);
                        if (part >= capacity - length) {
                            ok = false;
                            break;
                        }
                        memcpy(sprite + length, parser.data, part + 1u);
                        length += part;
                    }
                    break;

                case YXML_ATTREND:
                    readingSprite = false;
                    break;

                case YXML_ELEMEND:
                    if (!strcmp(parser.elem, "character"))
                        inCharacterTag = false;
                    break;

                default:
                    break;
            }
        }
    }

    ok = ok && !ferror(file) && yxml_eof(&parser) >= 0;
    fclose(file);
    return ok && foundRoot;
}

bool Char_FindCharacterXML(const CharCtx *ctx, const char *name,
                                  char xmlPath[CHAR_PATH_MAX], char spriteName[128])
{
    static const char *const formats[] = {
        "characters/%s.xml",
        "data/characters/%s.xml",
        "images/characters/%s.character.xml",
        "images/characters/%s-char.xml",
        "images/characters/%s_char.xml",
        "images/characters/%s.xml"
    };

    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        char candidate[CHAR_PATH_MAX];
        char rawSprite[128];

        if (!Char_Find(ctx, candidate, formats[i], name) ||
            !Char_ReadXMLSprite(candidate, rawSprite, sizeof(rawSprite))) {
            continue;
        }

        Char_CopyString(xmlPath, CHAR_PATH_MAX, candidate);
        Char_StemName(rawSprite[0] ? rawSprite : name, spriteName, 128);
        if (!Char_IsSafeName(spriteName))
            Char_CopyString(spriteName, 128, name);
        return true;
    }
    return false;
}

bool Char_FindAtlasXML(const CharCtx *ctx, const char *sprite, const char *name,
                              const char *characterXML, char atlasPath[CHAR_PATH_MAX])
{
    static const char *const formats[] = {
        "images/characters/%s.xml",
        "images/characters/%s.atlas.xml",
        "images/characters/%s-atlas.xml",
        "images/characters/%s_atlas.xml",
        "images/characters/%sAtlas.xml"
    };
    const char *const names[2] = { sprite, name };

    for (int n = 0; n < 2; ++n) {
        if (Char_SkipName(names, n))
            continue;

        for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            char candidate[CHAR_PATH_MAX];
            /* o proprio XML do personagem nao serve de atlas */
            if (Char_Find(ctx, candidate, formats[i], names[n]) &&
                strcmp(candidate, characterXML) != 0) {
                Char_CopyString(atlasPath, CHAR_PATH_MAX, candidate);
                return true;
            }
        }
    }
    return false;
}

bool Char_FindCharacterPNG(const CharCtx *ctx, const char *sprite, const char *name,
                                  char pngPath[CHAR_PATH_MAX])
{
    const char *const names[2] = { sprite, name };

    for (int i = 0; i < 2; ++i) {
        if (!Char_SkipName(names, i) &&
            Char_Find(ctx, pngPath, "images/characters/%s.png", names[i])) {
            return true;
        }
    }
    return false;
}

static void Char_ApplyXMLMetadata(const CharCtx *ctx, Character *character,
                                  RayCharacterXMLData *data,
                                  const char *name, CharRole role)
{
    character->object.animationSet = data->animations;
    data->animations = (RayAnimationHandler){0};

    const float scale = data->hasScale ? data->scale : 1.0f;
    character->object.scaleX = data->flipX ? -scale : scale;
    character->object.scaleY = scale;

    if (data->hasX)
        character->object.position.x = data->x;
    if (data->hasY)
        character->object.position.y = data->y;
    if (data->hasColor)
        character->object.color = data->color;
    if (data->holdTime > 0.0f)
        character->singDuration = data->holdTime;

    Vector2 camera = Char_DefaultCamera(role);
    if (data->hasCameraX)
        camera.x = data->cameraX;
    if (data->hasCameraY)
        camera.y = data->cameraY;
    character->cameraOffset = camera;

    Char_ApplyXMLAnimationSlots(character, &character->object.animationSet);
    AnimatedObject_SetAnimation(&character->object, character->animations.idle);

    if (role != CHAR_GF)
        Char_LoadIcon(ctx, character, name, NULL, role);
}

bool Char_TryLoadXMLCharacter(const CharCtx *ctx, Character *character,
                                     const char *name, CharRole role)
{
    char characterXML[CHAR_PATH_MAX];
    char atlasXML[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];
    char spriteName[128];

    if (!Char_FindCharacterXML(ctx, name, characterXML, spriteName) ||
        !Char_FindAtlasXML(ctx, spriteName, name, characterXML, atlasXML) ||
        !Char_FindCharacterPNG(ctx, spriteName, name, pngPath)) {
        return false;
    }

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayCharacterXMLData data = {0};
    if (!AnimationSet_LoadCharacterXML(characterXML, atlasXML, texture, &data)) {
        UnloadTexture(texture);
        return false;
    }

    if (!data.antialiasing)
        SetTextureFilter(data.animations.texture, TEXTURE_FILTER_POINT);

    Char_ApplyXMLMetadata(ctx, character, &data, name, role);
    return true;
}

