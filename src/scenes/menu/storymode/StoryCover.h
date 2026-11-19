/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * StoryCover.h - capa da week selecionada no Story Mode.
 *
 * Convencao das capas:
 *   assets/mods/<mod>/images/menubackgrounds/menu_<chave>.png
 *   assets/images/menubackgrounds/menu_<chave>.png
 */
#pragma once
#ifndef STORYCOVER_H
#define STORYCOVER_H

#include "raylib.h"
#include "data/Week.h"

/* Nova lista de weeks: descarrega a capa e zera o cache de caminhos. */
void StoryCover_Reset(int weekCount);

/* Carrega a capa da week `index` (nao faz nada se ja e a atual). Le disco: chame so ao trocar de week. */
void StoryCover_Load(int index, const WeekEntry* week);

void StoryCover_Free(void);

char StoryCover_IsLoaded(void);
Texture2D StoryCover_Texture(void);

#endif
