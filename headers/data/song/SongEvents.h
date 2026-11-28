/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SongEvents.h - leitura dos eventos no formato Psych (events.json e chart), usada pela gameplay e pelo editor.
 *
 * Aceita: {"events": [...]}, {"song": {"events": [...]}}, item [tempo, [[nome, v1, v2, ...], ...]]
 * ou [tempo, nome, v1, v2], e notas legadas (casa < 0: [tempo, -1, nome, v1, v2]) em notes[].sectionNotes.
 */
#pragma once
#ifndef SONG_EVENTS_H
#define SONG_EVENTS_H

#include <stddef.h>
#include "yyjson.h"

/* Um evento: vals[0..3] = posicoes 1..4 do evento (value1, value2, value3, s1). NULL = ausente. */
typedef void (*SongEventsFn)(void* user, double timeMs, const char* name, yyjson_val* const vals[4]);

/* Eventos de um documento ja lido. Retorna quantos entregou. */
int SongEvents_Read(yyjson_val* root, SongEventsFn fn, void* user);

/* Le o arquivo (ausente = 0 eventos). quickCheck = 1 (chart grande): so faz o parse se o texto
 * tem "events" ou cara de nota legada. */
int SongEvents_ReadFile(const char* path, char quickCheck, SongEventsFn fn, void* user);

/* Eventos do chart: "events"/notas legadas dentro de chartPath e, se <songDir>/events.json nao existe,
 * o events.json da pasta do chart (layout Psych data/<song>/). */
int SongEvents_ReadChartExtras(const char* songDir, const char* chartPath, SongEventsFn fn, void* user);

/* Valor como texto (string, numero ou bool); "" se ausente. */
void SongEvents_ValStr(yyjson_val* v, char* out, size_t n);

#endif /* SONG_EVENTS_H */
