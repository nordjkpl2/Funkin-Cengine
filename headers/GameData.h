/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "raylib.h"

#define MENU_MUSIC_NAME_MAX 64

/* Opcoes do jogador (salvas em assets/save.data). Para criar uma opcao nova:
   campo aqui, valor padrao em GameData_Defaults() e leitura/escrita em GameData.c. */
typedef struct GameOptions {
    char menuMusic[MENU_MUSIC_NAME_MAX]; /* faixa de menu escolhida (nome sem extensao); vazio = padrao */
    char showScore, bopIcons, vsync, fillScreen;
    char botplay, ghost, scroll, hitSound, showFps;
    char scorePosition; /* SCORE_POS_CENGINE ou SCORE_POS_PSYCH */
    char healthDrainPlay;     /* oponente acerta -> perde vida no jogo normal (padrao: off) */
    char healthDrainBotplay;  /* botplay ganha/perde vida como o jogador (padrao: off; off = barra travada) */
    char strumBgPercent;      /* fundo preto atras dos receptores: 0 = off, 10..100 = opacidade em % */
    char strumMobileVisible;  /* 1 = tela dividida em 4 colunas com as cores das notas */
    unsigned char laneTint[4][3]; /* RGB por coluna (esq, baixo, cima, dir): multiplica a skin; 255,255,255 = original */
    int scheme, fps;
    int threadLimit; /* 0 = Auto (nucleos - 1); >0 = limite de threads de fundo (1..64) */
    float zoomFactorUI, zoomFactorGAME;
} GameOptions;

/* Onde o score aparece no HUD: canto inferior direito (Cengine) ou centro sob a barra (Psych). */
#define SCORE_POS_CENGINE 0
#define SCORE_POS_PSYCH   1

extern GameOptions gameOptions;

/* Cor de tint de uma coluna (0..3), ja com alpha 255. Branco = sem mudanca. */
Color GameData_LaneTint(int lane);

/* Esquemas de teclas: Arrows, WASD, DFJK, ASKL e Custom (teclas do jogador, salvas). */
#define SCHEME_COUNT 5
#define SCHEME_CUSTOM 4
extern const char* game_available_schemes[SCHEME_COUNT];
extern int         game_scheme_keys[SCHEME_COUNT][4];   /* a linha SCHEME_CUSTOM e editavel */

/* Mods desativados pelo jogador (salvos no save v8). Mod novo ja nasce ativo. */
#define MOD_NAME_MAX 64
#define MOD_DISABLED_MAX 128
extern char gameDisabledMods[MOD_DISABLED_MAX][MOD_NAME_MAX];
extern int  gameDisabledModCount;

void GameData_Start(void);
void GameData_Flush(void);
void GameData_Stop(void);
#endif
