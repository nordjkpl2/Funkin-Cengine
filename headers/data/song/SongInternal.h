/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongInternal.h - funcoes compartilhadas pelos modulos de carregamento de musica
 * (Song.c, SongJson.c, SongStream.c, SongChartFind.c). Nao e API publica: use data/Song.h.
 */
#pragma once
#ifndef SONGINTERNAL_H
#define SONGINTERNAL_H

#include "data/song/Song.h"
#include "yyjson.h"
#include <stddef.h>

#define SONG_PATH_MAX       1024
#define SONG_MAX_SECTIONS   100000

/* SongJson.c */
void Song_CopyString(char dst[SONG_NAME_MAX], const char *src, const char *fallback);
int Song_ReadFile(const char *path, char **outBuffer, size_t *outSize);
char Song_ParseJSON(Song *song, const char *jsonPath, char streaming);

/* Cabecalho do chart (nomes, skins, bpm, speed) a partir do objeto "song" (ou da raiz). */
void Song_ReadHeader(yyjson_val *songObject, Song *song);
/* Meta de uma secao (tamanho, bpm, mustHit). *runningBpm acompanha as mudancas de bpm. */
void Song_ReadSectionMeta(yyjson_val *sectionValue, float *runningBpm, Section *section);
/* Notas de uma secao (lista de {tempo, lane, comprimento}) -> notas ordenadas, sem repeticao de lane.
 * Retorna 0 sem notas validas (outNotes = NULL, outCount = 0); senao *outNotes e liberado com free. */
char Song_ConvertSectionNotes(yyjson_val *notesValue, char mustHit, DataNote **outNotes, int *outCount);

/* SongChartFind.c */
const char *Song_BaseName(const char *path);
void Song_NormalizePath(const char *src, char *out, size_t outSize);
void Song_GetModRoot(const char *songDir, char *out, size_t outSize);
int Song_TryLoadChart(const char *dirPath, const char *diff, Song *song, char streaming);
int Song_ResolveChart(const char *dirPath, const char *diff, char out[SONG_PATH_MAX]);

#endif
