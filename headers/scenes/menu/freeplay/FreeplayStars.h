/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayStars.h - dificuldade da musica no Freeplay, em estrelas (mais notas do jogador = mais estrelas).
 */
#pragma once
#ifndef FREEPLAYSTARS_H
#define FREEPLAYSTARS_H

#include "raylib.h"
#include "data/song/SongList.h"
#include <stdbool.h>

#define FREEPLAY_STARS_MAX 10

/* Prepara o cache para songs[0..count-1] (ponteiros devem vir dessa lista). Solta o anterior. */
void FreeplayStars_Init(const SongEntry* songs, int count);
/* Espera as contagens em voo e solta o cache. */
void FreeplayStars_Unload(void);

/* Pede a contagem em fundo (nao bloqueia). */
void FreeplayStars_Request(const SongEntry* song);
/* Notas do jogador: -2 = contando, -1 = sem chart, >= 0 = contagem. */
int FreeplayStars_Notes(const SongEntry* song);
/* Estrelas (0..FREEPLAY_STARS_MAX, passos de 0.5) para uma contagem de notas. */
float FreeplayStars_Rating(int notes);

/* Fileira de estrelas: stars cheias (passo 0.5), o resto apagado. Cor vai de ciano a vermelho com a nota. */
void FreeplayStars_Draw(Vector2 pos, float size, float stars, float rating, float alpha);

#endif
