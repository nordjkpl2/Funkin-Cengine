/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "raylib.h"

/* Opcoes do jogador (salvas em assets/save.data). Para criar uma opcao nova:
   campo aqui, valor padrao em GameData_Defaults() e leitura/escrita em GameData.c. */
typedef struct GameOptions {
    char showScore, bopIcons, vsync, fillScreen;
    char botplay, ghost, scroll, hitSound, showFps;
    int scheme, fps;
    int threadLimit; /* 0 = Auto (nucleos - 1); >0 = limite de threads de fundo (1..64) */
    float zoomFactorUI, zoomFactorGAME;
} GameOptions;

extern GameOptions gameOptions;

/* Esquemas de teclas: Arrows, WASD, DFJK, ASKL e Custom (teclas do jogador, salvas). */
#define SCHEME_COUNT 5
#define SCHEME_CUSTOM 4
extern const char* game_available_schemes[SCHEME_COUNT];
extern int         game_scheme_keys[SCHEME_COUNT][4];   /* a linha SCHEME_CUSTOM e editavel */

void GameData_Start(void);
void GameData_Flush(void);
void GameData_Stop(void);
#endif
