/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "raylib.h"

#define MENU_MUSIC_NAME_MAX 64

/* Opcoes do jogador (salvas em assets/save.data). Para criar uma opcao nova (escalar):
   campo aqui e UMA linha no fim de saveFields (GameData.c) com id novo, padrao e faixa. Ids nunca reaproveitados. */
typedef struct GameOptions {
    char menuMusic[MENU_MUSIC_NAME_MAX]; /* faixa de menu escolhida (nome sem extensao); vazio = padrao */
    char showScore, bopIcons, vsync, fillScreen;
    char botplay, ghost, scroll, hitSound, showFps;
    char scorePosition; /* SCORE_POS_CENGINE ou SCORE_POS_PSYCH */
    char healthDrainPlay;     /* oponente acerta -> perde vida no jogo normal (padrao: off) */
    char healthDrainBotplay;  /* botplay ganha/perde vida como o jogador (padrao: off; off = barra travada) */
    char streamingJsonParse;  /* chart JSON em janelas por secao (streaming): menos RAM em charts grandes (padrao: off) */
    char shaders;             /* efeitos de tela dos eventos (TV, Glitch, Blackness, Grayscale): 0 = off, 1 = on (padrao: on) */
    char hitSoundBotplay;     /* som de acerto tambem quando o botplay acerta (padrao: off) */
    char noteSplashes;        /* splash nos receptores ao acertar uma nota (padrao: on) */
    char strumBgPercent;      /* fundo preto atras dos receptores: 0 = off, 10..100 = opacidade em % */
    char strumMobileVisible;  /* 1 = tela dividida em 4 colunas com as cores das notas */
    char hiddenNotesAlt;      /* Hidden Notes: 0 Off, 1 Alternate, 2 Only Playable, 3 Alternate Playable (ver GameData_HiddenShowsSide) */
    char loseIcon;            /* Losing Icon: icone troca para o de derrota quando o lado perde (so visual) */
    char horizontalMenu;      /* menu de opcoes: 0 = lista vertical, 1 = titulos em barra horizontal (uma secao por vez) */
    short laneHSB[4][3];      /* por coluna (esq, baixo, cima, dir): offset de matiz (-180..180 graus), saturacao e brilho (-100..100 %); 0 = skin original */
    int scheme, fps;
    int threadLimit; /* 0 = Auto (nucleos - 1); >0 = limite de threads de fundo (1..64) */
    float zoomFactorUI, zoomFactorGAME;
} GameOptions;

/* Onde o score aparece no HUD: canto inferior direito (Cengine) ou centro sob a barra (Psych). */
#define SCORE_POS_CENGINE 0
#define SCORE_POS_PSYCH   1

/* Estilo de scroll (gameOptions.scroll): Middledown = middlescroll + downscroll ao mesmo tempo. */
#define SCROLL_UP         0
#define SCROLL_DOWN       1
#define SCROLL_MIDDLE     2
#define SCROLL_MIDDLEDOWN 3
#define SCROLL_IS_DOWN(s)   ((s) == SCROLL_DOWN || (s) == SCROLL_MIDDLEDOWN)
#define SCROLL_IS_MIDDLE(s) ((s) == SCROLL_MIDDLE || (s) == SCROLL_MIDDLEDOWN)

extern GameOptions gameOptions;

/* Offset HSB de uma coluna (0..3) para o shader: x = matiz / 360, y = saturacao / 100, z = brilho / 100.
   (0,0,0) = sem mudanca. */
Vector3 GameData_LaneShift(int lane);
/* Mesma conta do shader, na CPU (para a cor de um monitor/icone). c e a cor base. */
Color GameData_ShiftColor(Color c, Vector3 shift);
/* 1 se a coluna tem algum offset (0 = skin original). */
char GameData_LaneShifted(int lane);

/* Hidden Notes: 1 se o lado (bfSide: 1 = BF) deve ser desenhado; singerBf = quem canta na secao. */
char GameData_HiddenShowsSide(char bfSide, char singerBf);

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

/* Volta as opcoes para o padrao (mantem mods desativados e teclas do esquema Custom). Nao grava sozinho. */
void GameData_ResetOptions(void);

void GameData_Start(void);
void GameData_Flush(void);
void GameData_Stop(void);
#endif
