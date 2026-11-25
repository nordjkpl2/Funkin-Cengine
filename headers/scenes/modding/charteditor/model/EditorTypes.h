/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: tipos do chart (notas, trechos, eventos). Base de todos os modulos de data/. */
#pragma once
#ifndef EDITOR_TYPES_H
#define EDITOR_TYPES_H

#include "std.h"
#include "Render.h"
#include "data/song/Song.h"

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
    EdNote* notes; // ordenadas por tempo
    int count;
    int cap;
    float tmin, tmax; // limites de tempo das notas (so cresce; so vale com count > 0)
    float lenMax;     // maior nota longa do trecho (so cresce): quanto olhar pra tras ao desenhar rastros
} EdSec;

typedef struct {
    char name[64];
    char p1[SONG_NAME_MAX];
    char p2[SONG_NAME_MAX];
    char stage[SONG_NAME_MAX];
    char gf[SONG_NAME_MAX];       // gfVersion (Girlfriend): "gf", "none"/"nogf" = sem girlfriend
    char noteSkin[SONG_NAME_MAX]; // custom note skin (mod > global default)
    char dir[256];                // pasta da musica ("" = ainda nao salva)
    float bpm;
    float speed;
    EdSec* secs;
    int nsec;
    char dirty;
} EdChart;

// evento da musica (events.json). Formato: [tempo, [[nome, v1, v2, v3, s1]]] (v3/s1 so em Show Image)
// fade: nome "Fade In"/"Fade Out", v1 = duracao (ms), v2 = alpha alvo (%)
// Show Image: v1 = x, v2 = y (px a partir do centro), v3 = escala (%), s1 = nome da imagem
typedef struct {
    float time; // ms
    char name[48];
    char v1[64];
    char v2[64];
    char v3[16];
    char s1[64];
} EdEvent;

#endif
