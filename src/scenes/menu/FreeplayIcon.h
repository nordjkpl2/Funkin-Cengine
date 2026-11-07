/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayIcon.h - icone da musica selecionada no Freeplay (busca em mods e base).
 */
#pragma once
#ifndef FREEPLAYICON_H
#define FREEPLAYICON_H

#include "raylib.h"
#include "data/SongList.h"

/* Resolve e carrega o icone da musica (descarrega o anterior). NULL so descarrega.
 * Chame apenas quando a selecao mudar: a busca le disco. Falhas tambem ficam em cache. */
void FreeplayIcon_Load(SongEntry* song);
void FreeplayIcon_Unload(void);

char FreeplayIcon_IsLoaded(void);
Texture2D FreeplayIcon_Texture(void);

/* Oponente da musica carregada ("unknown" se nao achou). */
const char* FreeplayIcon_Opponent(void);

#endif
