/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteAtlas.h - atlas das cores de lane da skin de nota.
 *
 * Gera, uma vez por musica, a skin recolorida das 4 lanes lado a lado numa textura so.
 * As notas passam a desenhar com a mesma textura, sem shader por nota: o lote de desenho
 * fica unico. Sem cor customizada (todas as lanes brancas) ou se a skin nao cabe, nao ha
 * atlas e a gameplay usa o caminho com shader.
 */
#pragma once
#ifndef PLAYSTATE_NOTEATLAS_H
#define PLAYSTATE_NOTEATLAS_H

#include <stdbool.h>
#include "Render.h"

/* Monta o atlas a partir da skin base. shifts[lane] = offset HSB da lane (GameData_LaneShift). false = sem atlas. */
bool NoteAtlas_Build(const RayAnimationHandler* base, const Vector3 shifts[4]);

/* Conjunto de animacao da variante da lane (mesmos indices da skin base). NULL sem atlas. */
const RayAnimationHandler* NoteAtlas_Lane(int lane);

void NoteAtlas_Free(void);

#endif
