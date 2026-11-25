/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * EffectsConfig.h - ajustes visuais editaveis (splash, popups, camera base).
 * Arquivo: assets/effects.json. Gravado pelo Effects Editor; lido uma vez e mantido em memoria.
 */
#pragma once
#ifndef DATA_EFFECTS_CONFIG_H
#define DATA_EFFECTS_CONFIG_H

#include "raylib.h"

/* Mesma ordem de PlayerRating: 0 shit, 1 bad, 2 good, 3 sick. */
#define EFFECTS_RATINGS 4
#define EFFECTS_TINTS 6

typedef struct {
    int scalePct;   /* tamanho do splash, 80 = padrao */
    int alphaPct;   /* opacidade, 60 = padrao */
    int fps;        /* velocidade da animacao, 24 = padrao */
    int offsetX;    /* deslocamento em px a partir do receptor */
    int offsetY;
    int tint;       /* indice em EffectsConfig_TintColor */
} EffectsSplash;

typedef struct {
    int scalePct;   /* 60 = padrao */
    int offsetX;    /* px a partir da posicao padrao do popup */
    int offsetY;
} EffectsPopup;

typedef struct {
    int zoomPct;    /* zoom base da camera de jogo, 100 = padrao */
    int offsetX;    /* deslocamento base do alvo da camera, em px do mundo */
    int offsetY;
} EffectsCamera;

typedef struct {
    EffectsSplash splash;
    EffectsPopup popups[EFFECTS_RATINGS];
    EffectsCamera camera;
} EffectsConfig;

/* Config em uso. Carrega assets/effects.json na primeira chamada (ausente = padroes). */
const EffectsConfig* EffectsConfig_Get(void);
/* Troca a config em memoria (usado pelo editor antes de gravar). */
void EffectsConfig_Set(const EffectsConfig* cfg);
/* Grava assets/effects.json. Retorna 1 se gravou. */
char EffectsConfig_Save(void);
/* Preenche com os valores padrao do engine. */
void EffectsConfig_Defaults(EffectsConfig* out);
/* Cor de tint do splash (indice 0..EFFECTS_TINTS-1, fora da faixa = branco). */
Color EffectsConfig_TintColor(int index);
/* Nome do tint para o editor. */
const char* EffectsConfig_TintName(int index);

#endif /* DATA_EFFECTS_CONFIG_H */
