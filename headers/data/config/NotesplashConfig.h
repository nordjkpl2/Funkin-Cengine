/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NotesplashConfig.h - lista de notesplashes (atlas Sparrow + animacao) e qual esta ativa na gameplay.
 * Arquivo: assets/notesplashes.json. Gravado pelo Notesplash Editor; lido uma vez e mantido em memoria.
 */
#pragma once
#ifndef DATA_NOTESPLASH_CONFIG_H
#define DATA_NOTESPLASH_CONFIG_H

#include <stddef.h>

#define NOTESPLASH_MAX 16
#define NOTESPLASH_PATH_MAX 128
#define NOTESPLASH_PREFIX_MAX 64

typedef struct {
    char animset[NOTESPLASH_PATH_MAX];   /* xml Sparrow, ex.: assets/images/noteSplashes-sparkles.xml */
    char prefix[NOTESPLASH_PREFIX_MAX];  /* prefixo da animacao sem os digitos finais, ex.: "splash sparkles red " */
    int tint;                            /* -1 = usa o tint do Effects Editor; senao indice em EffectsConfig_TintColor */
} NotesplashEntry;

typedef struct {
    NotesplashEntry entries[NOTESPLASH_MAX];
    int count;   /* 1..NOTESPLASH_MAX */
    int active;  /* indice em entries usado pela gameplay */
} NotesplashList;

/* Lista padrao do engine (o splash sparkles vermelho de sempre). */
void NotesplashConfig_Defaults(NotesplashList* out);
/* Lista em uso. Carrega assets/notesplashes.json na primeira chamada (ausente/invalido = padrao). */
const NotesplashList* NotesplashConfig_Get(void);
/* Troca a lista em memoria (usado pelo editor antes de gravar). */
void NotesplashConfig_Set(const NotesplashList* list);
/* Grava assets/notesplashes.json. Retorna 1 se gravou. */
char NotesplashConfig_Save(void);
/* Entrada ativa na gameplay. */
const NotesplashEntry* NotesplashConfig_Active(void);
/* Caminho da textura: troca ".xml" por ".png" no caminho do atlas. Retorna 0 se nao couber. */
char NotesplashConfig_TexturePath(const char* animset, char* out, size_t size);

#endif /* DATA_NOTESPLASH_CONFIG_H */
