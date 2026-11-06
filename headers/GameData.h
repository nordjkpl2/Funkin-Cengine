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
    float zoomFactorUI, zoomFactorGAME;
} GameOptions;

extern GameOptions gameOptions;

extern const char* game_available_schemes[4];
extern const int   game_scheme_keys[4][4];

void GameData_Start(void);
void GameData_Flush(void);
void GameData_Stop(void);
#endif
