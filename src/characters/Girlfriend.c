/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Girlfriend.c - Carregamento da namorada (GF) e poses de danca.
 */

#include "characters/Character.h"
#include "characters/CharacterInternal.h"
#include "logic/animations/CharacterAnims.h"
#include "characters/CharacterJson.h"
#include "characters/CharacterXML.h"
#include "core/Log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* API: girlfriend                                                           */
/* ------------------------------------------------------------------------- */

void Girlfriend_Load(Girlfriend *restrict gf, const char *restrict name)
{
    if (!gf)
        return;

    const char *characterName = (name && Char_IsSafeName(name)) ? name : "gf";
    CharCtx resolvedCtx;
    Char_ResolveCtx(&resolvedCtx, characterName);
    const CharCtx *ctx = &resolvedCtx;

    Render_DefaultAnimated(&gf->object);
    gf->f = 0;
    gf->namedCount = 0;

    /* XML: mesma descoberta dos outros personagens, sem carregar icone. */
    Character temporary = {0};
    Render_DefaultAnimated(&temporary.object);
    Char_InitAnimationSlots(&temporary);

    if (Char_TryLoadXMLCharacter(ctx, &temporary, characterName, CHAR_GF)) {
        static const char *const leftNames[] = { "danceLeft", "danceLEFT", "idle", "GF Dancing Beat0" };
        static const char *const rightNames[] = { "danceRight", "danceRIGHT", "idle", "GF Dancing Beat1" };

        gf->object = temporary.object;
        gf->anims[0] = CHAR_FIND_ANY(&gf->object.animationSet, leftNames);
        gf->anims[1] = CHAR_FIND_ANY(&gf->object.animationSet, rightNames);

        if (gf->anims[0] < 0 && gf->object.animationSet.animationCount > 0)
            gf->anims[0] = 0;
        if (gf->anims[1] < 0)
            gf->anims[1] = gf->anims[0];

        AnimatedObject_SetAnimation(&gf->object, gf->anims[0]);
        return;
    }

    /* animset + png (+ characters/<nome>.json); sem eles, cai na gf padrao */
    char setPath[CHAR_PATH_MAX];
    char jsonPath[CHAR_PATH_MAX];
    char pngPath[CHAR_PATH_MAX];

    CharJson json;
    Char_JsonInit(&json);

    if (Char_Find(ctx, jsonPath, "characters/%s.json", characterName))
        Char_ReadJson(jsonPath, &json);

    const char *textureName = json.imageBase[0] ? json.imageBase : characterName;
    bool found = Char_FindPair(ctx, setPath, pngPath, characterName, textureName) ||
                 (strcmp(textureName, characterName) != 0 &&
                  Char_FindPair(ctx, setPath, pngPath, characterName, characterName));
    if (!found && strcmp(characterName, "gf") != 0) {
        Log_WarnF("Girlfriend '%s' nao encontrada: usando gf padrao", characterName);
        Char_JsonInit(&json);
        found = Char_FindPair(ctx, setPath, pngPath, "gf", "gf");
    }
    if (!found)
        Log_Fatal("Girlfriend_Load: could not locate girlfriend assets");

    Texture2D texture = Render_LoadTexture(pngPath);
    if (!texture.id)
        Log_Fatal("Girlfriend_Load: could not load girlfriend texture");

    RayAnimationHandler set = AnimationSet_LoadAnimations(setPath, texture);

    /* Danca: danceLeft/danceRight do JSON; senao idle nas duas; senao as 2 primeiras. */
    int left = Char_BindRole(&set, &json, "danceLeft");
    int right = Char_BindRole(&set, &json, "danceRight");
    const int idle = Char_BindRole(&set, &json, "idle");
    if (left < 0) left = idle;
    if (right < 0) right = left;

    if (left < 0) {
        for (int i = 0; i < set.animationCount && i < 2; ++i) {
            set.animations[i].fps = 24.0f;
            set.animations[i].frameTime = 1.0f / 24.0f;
            set.animations[i].looped = false;
        }
        left = 0;
        right = set.animationCount > 1 ? 1 : 0;
    }

    const int bound[3] = { left, right, idle };
    gf->namedCount = Char_BindNamedRoles(&set, &json, gf->named, CHAR_NAMED_MAX, bound, 3);
    gf->object.animationSet = set;
    gf->anims[0] = left;
    gf->anims[1] = right;
    gf->object.scaleX = json.flipX ? -json.scale : json.scale;
    gf->object.scaleY = json.scale;
    gf->object.color = WHITE;
    Vector2 gfBox, gfShift;
    Char_PsychBox(pngPath, json.scale, &gfBox, &gfShift);
    gf->object.position = (Vector2){ json.position.x + gfShift.x, json.position.y + gfShift.y };
    AnimatedObject_SetAnimation(&gf->object, gf->anims[0]);
}

