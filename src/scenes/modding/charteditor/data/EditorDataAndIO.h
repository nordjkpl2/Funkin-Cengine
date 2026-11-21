/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: dados do chart (notas, trechos, undo) e leitura/gravacao em arquivo. */
#pragma once
#ifndef EDITOR_DATA_AND_IO_H
#define EDITOR_DATA_AND_IO_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"

typedef struct {
    float time; // ms
    int lane;   // 0-7 (0-3 oponente, 4-7 voce)
    float len;  // ms de nota longa, 0 = normal
} EdNote;

typedef struct {
    char mustHit; // 1 = sua vez, 0 = vez do oponente
    float bpm;
    char changeBPM;
    int lenSteps;
    EdNote* notes;
    int count;
    int cap;
    float tmin, tmax; // limites de tempo das notas (so cresce; so vale com count > 0)
} EdSec;

typedef struct {
    char name[64];
    char p1[SONG_NAME_MAX];
    char p2[SONG_NAME_MAX];
    char stage[SONG_NAME_MAX];
    char gf[SONG_NAME_MAX]; // gfVersion (Girlfriend): "gf", "none"/"nogf" = sem girlfriend
    char noteSkin[SONG_NAME_MAX]; // custom note skin (mod > global default)
    char dir[256]; // pasta da musica ("" = ainda nao salva)
    float bpm;
    float speed;
    EdSec* secs;
    int nsec;
    char dirty;
} EdChart;

// evento da musica (events.json). Formato Psych: [tempo, [[nome, v1, v2]]]
// fade: nome "Fade In"/"Fade Out", v1 = duracao (ms), v2 = alpha alvo (%)
typedef struct {
    float time; // ms
    char name[48];
    char v1[64];
    char v2[64];
} EdEvent;

extern EdChart chart;
extern char chartLoaded;
extern EdEvent* edEvents;
extern int edEventCount;
extern int edEventsGen; /* muda a cada limpeza/recarga: quem guarda indices deve reiniciar */
int EdEvent_Add(float time, const char* name, const char* v1, const char* v2); /* devolve o indice inserido, -1 = falhou */
void EdEvent_RemoveAt(int idx);
void EdEvent_ClearAll(void);
void EdEvents_Load(const char* dir);
char EdEvents_Save(const char* dir);
void EdChart_Free(EdChart* c);
void Undo_Clear(void);
void Undo_Pop(void);
void EdSec_Add(EdSec* s, float time, int lane, float len);
void EdSec_AddBulk(EdSec* s, float time, int lane, float len);
int EdDispLane(const EdSec* s, int lane);
int EdSec_FindNear(EdSec* s, float time, int lane, float windowMs);
char EdSec_RemoveAt(EdSec* s, int idx);
void EdChart_Default(EdChart* c, const char* name);
char EdChart_LoadJSON(EdChart* c, const char* jsonPath, const char* dir, const char* fallbackName);
const char* EdDirName(const char* path);
char EdChart_OpenDir(EdChart* c, const char* dir);
char EdChart_Save(const EdChart* c);
char EdChart_SaveCompressed(const EdChart* c);
char EdCopyFile(const char* src, const char* dst);
void EdSanitizeName(char* dst, size_t dstSize, const char* src);
extern char statusText[128];
extern float statusTimer;
void EdStatusMsg(const char* m);
void EdTouch(void);

#endif
