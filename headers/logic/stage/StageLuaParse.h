#pragma once
#ifndef STAGE_LUA_PARSE_H
#define STAGE_LUA_PARSE_H

#include "logic/stage/Stage.h"

/* Faz parse do subset de Lua suportado para out[capacity]. Retorna quantidade.
 * ignoredGlitch/ignoredAnim podem ser NULL.
 */
int Stage_ParseLua(const char *luaText, StageSprite *out, int capacity,
                   int *ignoredGlitch, int *ignoredAnim);

/* Igual, mas tambem guarda animacoes, playAnim das callbacks (onBeatHit, onStepHit,
 * onSectionHit, onCountdownTick) e ajustes do PlayState. script pode ser NULL. */
int Stage_ParseLuaScript(const char *luaText, StageSprite *out, int capacity, StageScript *script);

#endif /* STAGE_LUA_PARSE_H */
