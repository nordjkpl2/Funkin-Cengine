/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef NOTESKINCACHE_H
#define NOTESKINCACHE_H

#include "Render.h"

/* Skin padrao (carrega no primeiro uso) e skins de mod (cache por nome, max 16). */
RayAnimationHandler Cache_GetNoteAnimations(void);
RayAnimationHandler Cache_GetNoteSkin(const char* skinName, const char* songDir);
RayAnimationHandler* Cache_GetDefaultNoteSkinPtr(void);
/* Ponteiro estavel (slot do cache) da skin; skin vazia/inexistente = skin padrao. Use com Note_SetCurrentSkin. */
RayAnimationHandler* Cache_GetNoteSkinPtr(const char* skinName, const char* songDir);

/* Zera o cache (chamado no inicio do jogo). */
void NoteSkinCache_Reset(void);
/* Libera a skin padrao e todas as skins de mod (chamado no fim do jogo). */
void NoteSkinCache_Free(void);

#endif
