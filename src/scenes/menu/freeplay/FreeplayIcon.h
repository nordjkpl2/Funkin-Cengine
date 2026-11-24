/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayIcon.h - icone da musica selecionada no Freeplay (busca em mods e base).
 */
#pragma once
#ifndef FREEPLAYICON_H
#define FREEPLAYICON_H

#include "raylib.h"
#include "data/SongList.h"

/* Prepara o cache para a lista songs[0..count-1] (ponteiros devem vir dessa lista). */
void FreeplayIcon_Init(const SongEntry* songs, int count);

/* Carrega o icone da musica (descarrega o anterior). NULL so descarrega.
 * A busca em disco roda uma vez por musica (memo); a textura sai do cache se ja existir. */
void FreeplayIcon_Load(const SongEntry* song);
/* Pede a decodificacao em fundo do PNG da musica, sem subir para a GPU. Nao bloqueia. */
void FreeplayIcon_Warm(const SongEntry* song);
void FreeplayIcon_Unload(void);

/* Carrega o icone de uma musica sem mexer no icone unico (uso: lista do Chart Editor).
 * id 0 = nada achado. Quem chama faz UnloadTexture. */
Texture2D FreeplayIcon_Resolve(const SongEntry* song);

char FreeplayIcon_IsLoaded(void);
Texture2D FreeplayIcon_Texture(void);

/* Oponente da musica carregada ("unknown" se nao achou). */
const char* FreeplayIcon_Opponent(void);

#endif
