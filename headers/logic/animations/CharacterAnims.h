/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterAnims.h - Ligacao de papeis/builtins aos RayAnimationHandler.
 */
#pragma once
#ifndef CHARACTER_ANIMS_H
#define CHARACTER_ANIMS_H

#include <stdbool.h>
#include "characters/CharacterInternal.h"
#include "characters/CharacterJson.h"

/* ------------------------------------------------------------------------- */

typedef struct {
    const char *anim;        /* nome no atlas; NULL = sem animacao */
    float offsetX;
    float offsetY;
} BuiltinAnim;

typedef struct {
    const char *name;        /* arquivos: images/characters/<name>.animset/.png */
    BuiltinAnim idle;
    BuiltinAnim sing[4];     /* esquerda, baixo, cima, direita */
    BuiltinAnim miss[4];
} BuiltinDef;


#define CHAR_FIND_ANY(set, list) \
    Char_FindAnimationAny((set), (list), sizeof(list) / sizeof((list)[0]))

extern const BuiltinDef BUILTIN_BF;
extern const BuiltinDef BUILTIN_DAD;

int Char_FindAnimationAny(const RayAnimationHandler *set, const char *const *names, size_t count);
void Char_InitAnimationSlots(Character *character);
Vector2 Char_DefaultCamera(CharRole role);
void Char_ApplyXMLAnimationSlots(Character *character, const RayAnimationHandler *set);
int Char_BindRole(RayAnimationHandler *set, const CharJson *json, const char *role);
/* Papel -> indice de todos os papeis do JSON (Play Animation etc.); configura os que ainda nao estao em bound. */
int Char_BindNamedRoles(RayAnimationHandler *set, const CharJson *json, CharNamedAnim *out, int max,
                        const int *bound, int boundCount);
bool Char_LoadBuiltin(const CharCtx *ctx, Character *character, CharRole role);
bool Char_LoadLegacyCustom(const CharCtx *ctx, Character *character, const char *name, CharRole role);

#endif /* CHARACTER_ANIMS_H */
