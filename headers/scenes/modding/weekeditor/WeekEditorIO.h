/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Week Editor: lista, carregamento e salvamento das semanas (<raiz>/weeks/<arquivo>.json). */
#pragma once
#ifndef WEEKEDITORIO_H
#define WEEKEDITORIO_H

#include <stdbool.h>
#include <stddef.h>
#include "data/week/Week.h"
#include "scenes/modding/ModdingCommon.h"

#define WEEKED_LIST_MAX 256
#define WEEKED_TEXT_MAX 128

typedef struct {
    char file[MODED_NAME_MAX]; /* nome do arquivo sem .json */
    char root[MODED_ROOT_MAX]; /* raiz onde a semana mora */
    char title[WEEKED_TEXT_MAX];
} WeekEdEntry;

/* Musica da semana: pasta em songs/, icone e cor do freeplay (0..255). */
typedef struct {
    char name[64];
    char icon[64];
    int color[3];
} WeekEdSong;

typedef struct {
    char storyName[WEEKED_TEXT_MAX];
    char weekName[WEEKED_TEXT_MAX];
    char flavor[192];
    char difficulties[96];
    char background[64];
    char before[64];
    char chars[3][64];
    bool startUnlocked;
    bool hideStoryMode;
    bool hideFreeplay;
    int songCount;
    WeekEdSong songs[WEEK_MAX_SONGS];
} WeekEdDraft;

/* Semanas dos mods ativos e de assets. Devolve a quantidade. */
int WeekEd_ScanList(WeekEdEntry *out, int capacity);

/* Pastas de musica disponiveis (songs/<nome>) nos mods e em assets. */
int WeekEd_ScanSongs(char out[][MODED_NAME_MAX], int capacity);

/* Valores padrao para uma semana nova. */
void WeekEd_Defaults(WeekEdDraft *draft, const char *title);
void WeekEd_DefaultSong(WeekEdSong *song, const char *name);

bool WeekEd_Load(const WeekEdEntry *entry, WeekEdDraft *draft, char *msg, size_t msgCap);

/* Atualiza so os campos que o editor controla; o resto do JSON fica como estava. */
bool WeekEd_Save(const WeekEdEntry *entry, const WeekEdDraft *draft, char *msg, size_t msgCap);

#endif /* WEEKEDITORIO_H */
