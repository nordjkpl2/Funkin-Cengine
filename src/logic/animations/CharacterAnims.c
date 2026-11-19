/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterAnims.c - Ligacao entre defs de personagem e RayAnimationHandler (papeis, builtins, slots).
 */

#include "std.h"
#include "characters/Character.h"
#include "characters/CharacterInternal.h"
#include "logic/animations/CharacterAnims.h"
#include "characters/CharacterJson.h"
#include "characters/CharacterXML.h"
#include "core/ModPath.h"
#include "core/ModList.h"
#include "logic/animations/AnimationSets_XML.h"
#include "Log.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "yyjson.h"
#include "yxml.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Animacoes                                                                 */
/* ------------------------------------------------------------------------- */

int Char_FindAnimationAny(const RayAnimationHandler *set,
                                 const char *const *names, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        int index = AnimationSet_FindAnimation(set, names[i]);
        if (index >= 0)
            return index;
    }
    return -1;
}


/* Ajusta fps/loop/offset de uma animacao pelo nome. -1 se nao existe. */
static int Char_ConfigureAnimation(RayAnimationHandler *set, const char *name,
                                   float fps, bool looped,
                                   float offsetX, float offsetY)
{
    int index = name ? AnimationSet_FindAnimation(set, name) : -1;
    if (index < 0)
        return -1;

    RayAnimation *animation = &set->animations[index];
    if (fps <= 0.0f)
        fps = 24.0f;

    animation->fps = fps;
    animation->frameTime = 1.0f / fps;
    animation->looped = looped;
    animation->animationOffset = (Vector2){ offsetX, offsetY };
    return index;
}

void Char_InitAnimationSlots(Character *character)
{
    character->animations.idle = -1;
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = -1;
        character->missAnimations.notes[i] = -1;
    }
}

/* Camera padrao quando o personagem nao define uma. */
Vector2 Char_DefaultCamera(CharRole role)
{
    return role == CHAR_DAD ? (Vector2){ 400.0f, 300.0f }
                            : (Vector2){ 50.0f, 50.0f };
}

/* Nomes comuns de animacao (Codename / Psych) para cada pose. */
void Char_ApplyXMLAnimationSlots(Character *character,
                                        const RayAnimationHandler *set)
{
    static const char *const idleNames[] = { "idle", "dance", "danceIdle" };
    static const char *const singNames[4][3] = {
        { "singLEFT", "singLeft", "left" },
        { "singDOWN", "singDown", "down" },
        { "singUP", "singUp", "up" },
        { "singRIGHT", "singRight", "right" }
    };
    static const char *const missNames[4][4] = {
        { "singLEFTmiss", "singLeftMiss", "missLEFT", "singLEFT_miss" },
        { "singDOWNmiss", "singDownMiss", "missDOWN", "singDOWN_miss" },
        { "singUPmiss", "singUpMiss", "missUP", "singUP_miss" },
        { "singRIGHTmiss", "singRightMiss", "missRIGHT", "singRIGHT_miss" }
    };

    character->animations.idle = CHAR_FIND_ANY(set, idleNames);
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = CHAR_FIND_ANY(set, singNames[i]);
        character->missAnimations.notes[i] = CHAR_FIND_ANY(set, missNames[i]);
    }

    if (character->animations.idle < 0 && set->animationCount > 0)
        character->animations.idle = 0;
}

/* Liga o papel (ex.: "idle") a animacao do atlas; -1 se o JSON nao define. */
int Char_BindRole(RayAnimationHandler *set, const CharJson *json, const char *role)
{
    for (int i = 0; i < json->defCount; ++i) {
        const CharAnimDef *def = &json->defs[i];
        if (!strcmp(def->role, role)) {
            return Char_ConfigureAnimation(set, def->sprite, (float)def->fps,
                                           def->looped, def->offsetX, def->offsetY);
        }
    }
    return -1;
}

bool Char_LoadLegacyCustom(const CharCtx *ctx, Character *character,
                                  const char *name, CharRole role)
{
    char setPath[CHAR_PATH_MAX];
    char jsonPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    CharJson json;
    Char_JsonInit(&json);
    if (Char_Find(ctx, jsonPath, "characters/%s.json", name))
        Char_ReadJson(jsonPath, &json);

    const char *textureName = json.imageBase[0] ? json.imageBase : name;
    if (!Char_FindPair(ctx, setPath, pngPath, name, textureName) &&
        (!strcmp(textureName, name) || !Char_FindPair(ctx, setPath, pngPath, name, name))) {
        return false;
    }

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);
    if (json.defCount == 0)
        Char_AddDefaultDefs(&json);

    static const char *const singRoles[4] = { "singLEFT", "singDOWN", "singUP", "singRIGHT" };

    Char_InitAnimationSlots(character);
    character->animations.idle = Char_BindRole(&set, &json, "idle");
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = Char_BindRole(&set, &json, singRoles[i]);
        character->missAnimations.notes[i] = character->animations.notes[i];
    }
    if (character->animations.idle < 0 && set.animationCount > 0)
        character->animations.idle = 0;

    character->object.animationSet = set;
    /* Psych: o jogador inverte o flip_x do JSON (bf.json traz flip_x true e aparece sem espelhar). */
    const bool mirrored = (role == CHAR_BF) ? !json.flipX : json.flipX;
    character->object.scaleX = mirrored ? -json.scale : json.scale;
    character->object.scaleY = json.scale;
    character->object.color = WHITE;
    Char_PsychBox(pngPath, json.scale, &character->psychBox, &character->psychShift);
    character->object.position = (Vector2){ json.position.x + character->psychShift.x,
                                            json.position.y + character->psychShift.y };
    character->singDuration = json.singDuration;
    character->hasHealthColor = json.hasHealthColor;
    character->healthColor = json.healthColor;
    /* camera_position cru: PlayStateStage soma o ponto medio e as constantes da Psych. */
    character->cameraOffset = json.camera;
    character->psychCamera = true;

    AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    Char_LoadIcon(ctx, character, name, json.healthIcon, role);
    return true;
}

const BuiltinDef BUILTIN_BF = {
    "bf",
    { "BF idle dance", -5.0f, 0.0f },
    { { "BF NOTE LEFT", 5.0f, -6.0f },   { "BF NOTE DOWN", -20.0f, -51.0f },
      { "BF NOTE UP", -46.0f, 27.0f },   { "BF NOTE RIGHT", -48.0f, -7.0f } },
    { { "BF NOTE LEFT MISS", 7.0f, 19.0f },  { "BF NOTE DOWN MISS", -15.0f, -19.0f },
      { "BF NOTE UP MISS", -46.0f, 27.0f },  { "BF NOTE RIGHT MISS", -48.0f, 19.0f } }
};

const BuiltinDef BUILTIN_DAD = {
    "dad",
    { "Dad idle dance", 0.0f, 0.0f },
    { { "Dad Sing Note LEFT", -9.0f, 10.0f }, { "Dad Sing Note DOWN", 0.0f, -30.0f },
      { "Dad Sing Note UP", -6.0f, 50.0f },   { "Dad Sing Note RIGHT", 0.0f, 27.0f } },
    { { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f }, { NULL, 0.0f, 0.0f } }
};

static int Char_BindBuiltin(RayAnimationHandler *set, BuiltinAnim anim)
{
    return Char_ConfigureAnimation(set, anim.anim, 24.0f, false, anim.offsetX, anim.offsetY);
}

bool Char_LoadBuiltin(const CharCtx *ctx, Character *character, CharRole role)
{
    const BuiltinDef *def = (role == CHAR_DAD) ? &BUILTIN_DAD : &BUILTIN_BF;
    char setPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    if (!Char_FindPair(ctx, setPath, pngPath, def->name, def->name))
        return false;

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        return false;

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);

    Char_InitAnimationSlots(character);
    character->animations.idle = Char_BindBuiltin(&set, def->idle);
    for (int i = 0; i < 4; ++i) {
        character->animations.notes[i] = Char_BindBuiltin(&set, def->sing[i]);
        character->missAnimations.notes[i] = Char_BindBuiltin(&set, def->miss[i]);
    }
    if (character->animations.idle < 0 && set.animationCount > 0)
        character->animations.idle = 0;

    character->cameraOffset = Char_DefaultCamera(role);
    character->object.animationSet = set;
    character->object.scaleX = 1.0f;
    character->object.scaleY = 1.0f;
    character->object.color = WHITE;

    AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    Char_LoadIcon(ctx, character, def->name, NULL, role);
    return true;
}

