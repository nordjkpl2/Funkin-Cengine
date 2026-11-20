/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimationSets.c - Despacho de animsets: escolhe Sparrow XML ou binario e libera os recursos.
 */

#include "logic/animations/AnimSetParseUtil.h"
#include "logic/animations/AnimSetSparrow.h"
#include "logic/animations/AnimSetBinary.h"
#include "Log.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Public animation-set loader                                                */
/* ------------------------------------------------------------------------- */

RayAnimationHandler AnimationSet_LoadAnimations(const char *animset, Texture2D image)
{
    RayAnimationHandler result = {0};

    if (!animset) {
        Log_Fatal("AnimationSet_LoadAnimations: null path");
        return result;
    }

    bool loaded = AnimParse_IsLikelyXML(animset)
        ? Atlas_LoadSparrowXML(animset, image, &result)
        : Binary_LoadAnimSet(animset, image, &result);

    if (!loaded) {
        Log_FatalF("could not load animation set: %s", animset);
        return (RayAnimationHandler){0};
    }

    return result;
}

int AnimationSet_FindAnimation(const RayAnimationHandler *restrict animations,
                               const char *animationName)
{
    if (!animations || !animations->animations || !animationName)
        return -1;

    /* Nome repetido: vale a ultima (na Psych, addAnim com o mesmo nome substitui). */
    for (int i = animations->animationCount - 1; i >= 0; --i) {
        if (strcmp(animations->animations[i].name, animationName) == 0)
            return i;
    }

    return -1;
}

void AnimationSet_FreeAll(RayAnimationHandler *restrict animations)
{
    if (!animations) return;
    AnimationSet_FreeArrays(animations);
    if (animations->texture.id)
        UnloadTexture(animations->texture);
    animations->texture = (Texture2D){0};
}

