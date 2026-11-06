/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "raylib.h"

extern const char* game_available_schemes[4];
extern const int   game_scheme_keys[4][4];

extern char game_options_showScore, game_options_bopIcons, game_options_vsync, game_options_fillScreen;
extern char game_options_botplay, game_options_ghost, game_options_scroll, game_options_hitSound, game_options_showFps;
extern int  game_options_scheme, game_options_fps;
extern float game_options_zoomFactorUI, game_options_zoomFactorGAME; 

void GameData_Start(void);
void GameData_Flush(void);
void GameData_Stop(void);
#endif