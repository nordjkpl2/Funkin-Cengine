/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: eventos da musica (events.json). */
#pragma once
#ifndef EDITOR_EVENTS_H
#define EDITOR_EVENTS_H

#include "scenes/modding/charteditor/model/EditorTypes.h"

extern EdEvent* edEvents;
extern int edEventCount;
extern int edEventsGen; /* muda a cada limpeza/recarga: quem guarda indices deve reiniciar */

int EdEvent_Add(float time, const char* name, const char* v1, const char* v2, const char* v3, const char* s1); /* indice inserido, -1 = falhou */
void EdEvent_RemoveAt(int idx);
void EdEvent_ClearAll(void);
void EdEvents_Load(const char* dir);
/* Soma os eventos de dentro do chart (Psych: "events" e notas de casa -1). Chamar depois de EdEvents_Load. */
void EdEvents_AddFromChart(const char* dir, const char* chartPath);
char EdEvents_Save(const char* dir);

#endif
