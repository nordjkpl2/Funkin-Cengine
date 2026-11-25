/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayIcon.h - icone da musica selecionada no Freeplay (busca em mods e base).
 */
#pragma once
#ifndef FREEPLAYICON_H
#define FREEPLAYICON_H

#include "raylib.h"
#include "data/song/SongList.h"

/* Nada daqui bloqueia a thread principal: a busca do caminho roda em fundo (Jobs) e o PNG e
 * decodificado em fundo (ImagePrefetch); so o envio para a GPU acontece no frame. */

/* Prepara o cache para a lista songs[0..count-1] (ponteiros devem vir dessa lista). Solta o anterior. */
void FreeplayIcon_Init(const SongEntry* songs, int count);
void FreeplayIcon_Unload(void);

/* Musica atual (Freeplay): a troca e instantanea; FreeplayIcon_Tick termina o carregamento. NULL limpa. */
void FreeplayIcon_Load(const SongEntry* song);
/* Pede em fundo a resolucao e a decodificacao de uma musica vizinha. */
void FreeplayIcon_Warm(const SongEntry* song);
/* Chamar 1x por frame na thread principal. true = a musica atual ainda esta carregando (redesenhar). */
bool FreeplayIcon_Tick(void);
bool FreeplayIcon_Busy(void);
char FreeplayIcon_IsLoaded(void);
Texture2D FreeplayIcon_Texture(void);

/* Oponente da musica atual ("..." enquanto resolve, "unknown" se nao achou). */
const char* FreeplayIcon_Opponent(void);

/* Icone de qualquer musica da lista (Chart Editor). 1 = pronta (em *out; nao liberar),
 * 0 = carregando, -1 = sem icone. */
int FreeplayIcon_Get(const SongEntry* song, Texture2D* out);

#endif
