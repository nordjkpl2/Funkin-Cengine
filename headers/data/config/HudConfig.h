/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * HudConfig.h - layout do HUD do gameplay (posicao, escala e visibilidade de cada elemento).
 * Arquivo: assets/hud.json. Gravado pelo HUD Editor (menu de modding); lido uma vez e mantido em memoria.
 */
#pragma once
#ifndef DATA_HUD_CONFIG_H
#define DATA_HUD_CONFIG_H

typedef enum {
    HUD_EL_HEALTH = 0, /* barra de vida + icones (popups de rating seguem a barra) */
    HUD_EL_TIME,       /* tempo da musica (texto ou barra) */
    HUD_EL_SCORE,      /* score / misses / precisao */
    HUD_EL_TITLE,      /* nome da musica */
    HUD_EL_BOTPLAY,    /* marca BOTPLAY */
    HUD_ELEMENTS
} HudElementId;

typedef struct {
    int offsetX;    /* px a partir da posicao padrao */
    int offsetY;
    int scalePct;   /* 100 = padrao (barra de vida: escala dos icones) */
    char visible;
} HudElement;

typedef struct {
    HudElement el[HUD_ELEMENTS];
    int barWidthPct; /* largura da barra de vida, 100 = 600 px */
} HudConfig;

#define HUD_OFFSET_MAX 1000
#define HUD_SCALE_MIN 25
#define HUD_SCALE_MAX 300
#define HUD_WIDTH_MIN 25
#define HUD_WIDTH_MAX 200

/* Config em uso. Carrega assets/hud.json na primeira chamada (ausente = padroes). */
const HudConfig* HudConfig_Get(void);
/* Troca a config em memoria (editor, antes de gravar). */
void HudConfig_Set(const HudConfig* cfg);
/* Grava assets/hud.json. Retorna 1 se gravou. */
char HudConfig_Save(void);
void HudConfig_Defaults(HudConfig* out);
/* Nome legivel do elemento (editor). */
const char* HudConfig_Name(int id);

#endif /* DATA_HUD_CONFIG_H */
