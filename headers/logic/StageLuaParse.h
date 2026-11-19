#pragma once
#ifndef STAGE_LUA_PARSE_H
#define STAGE_LUA_PARSE_H

#include "logic/Stage.h"

/* Faz parse do subset de Lua suportado para out[capacity]. Retorna quantidade.
 * ignoredGlitch/ignoredAnim podem ser NULL.
 */
int Stage_ParseLua(const char *luaText, StageSprite *out, int capacity,
                   int *ignoredGlitch, int *ignoredAnim);

#endif /* STAGE_LUA_PARSE_H */
