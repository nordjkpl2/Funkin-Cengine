/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongInternal.h - funcoes compartilhadas pelos modulos de carregamento de musica
 * (Song.c, SongJson.c, SongChartFind.c). Nao e API publica: use data/Song.h.
 */
#pragma once
#ifndef SONGINTERNAL_H
#define SONGINTERNAL_H

#include "data/Song.h"
#include <stddef.h>

#define SONG_PATH_MAX       1024
#define SONG_MAX_SECTIONS   100000

/* SongJson.c */
void Song_CopyString(char dst[SONG_NAME_MAX], const char *src, const char *fallback);
char Song_ParseJSON(Song *song, const char *jsonPath);

/* SongChartFind.c */
const char *Song_BaseName(const char *path);
void Song_NormalizePath(const char *src, char *out, size_t outSize);
void Song_GetModRoot(const char *songDir, char *out, size_t outSize);
int Song_TryLoadChart(const char *dirPath, const char *diff, Song *song);
int Song_ResolveChart(const char *dirPath, const char *diff, char out[SONG_PATH_MAX]);

#endif
