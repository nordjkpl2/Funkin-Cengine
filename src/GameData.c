/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "GameData.h" 
#include "Log.h"
#include <stdio.h>

#define IERR Log_Fatal("[ SAVE ] invalid data");
#define READ(type, count, output) if(fread(&output, sizeof(type), count, saveFile) != count) IERR
#define WRITE(type, count, source) if(fwrite(&source, sizeof(type), count, saveFile) != count) Log_Fatal("[ SAVE ] unable to write to save file!");

#define SREAD(type, output) READ(type, 1, output)
#define SWRITE(type, source) WRITE(type, 1, source)

/* Declaração agrupada de variáveis do mesmo tipo */
char game_options_showScore, game_options_bopIcons, game_options_vsync, game_options_fillScreen;
char game_options_botplay, game_options_ghost, game_options_scroll, game_options_hitSound, game_options_showFps;
int game_options_scheme, game_options_fps;
float game_options_zoomFactorUI, game_options_zoomFactorGAME;

const char* game_available_schemes[4] = {"Arrows", "WASD", "DFJK", "ASKL"};
const int   game_scheme_keys[4][4] = {
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT},
    {KEY_A, KEY_S, KEY_W, KEY_D},
    {KEY_D, KEY_F, KEY_J, KEY_K},
    {KEY_A, KEY_S, KEY_K, KEY_L}
};

static FILE* saveFile;

#define SAVE_VERSION 5

static void GameData_Defaults(void) {
    game_options_showScore  = 1;
    game_options_bopIcons   = 1;
    game_options_vsync      = 0;
    game_options_fillScreen = 0;
    game_options_botplay    = 0;
    game_options_ghost      = 0; 
    game_options_scroll     = 0; 
    game_options_hitSound   = 0; 
    game_options_showFps    = 1; 
    game_options_fps        = 120;
    game_options_scheme     = 0;
    game_options_zoomFactorUI   = 1.0f;
    game_options_zoomFactorGAME = 1.0f;
}

static void GameData_Checks(void) {
    if(game_options_scheme < 0 || game_options_scheme > 3) IERR 
    if(game_options_fps < 0) IERR
    if(game_options_ghost < 0 || game_options_ghost > 2) IERR
    if(game_options_scroll < 0 || game_options_scroll > 2) IERR
    if(game_options_hitSound < 0 || game_options_hitSound > 1) IERR
    if(game_options_showFps < 0 || game_options_showFps > 1) IERR
}

void GameData_Start(void) {
    saveFile = fopen("assets/save.data", "r+b");
    if(saveFile == NULL) { 
        GameData_Defaults();
        saveFile = fopen("assets/save.data", "w+b");
        GameData_Flush();
        return;
    }

    char version = 0;
    SREAD(char, version)

    if(version != SAVE_VERSION) {
        GameData_Defaults();
        GameData_Flush();
        return;
    }

    SREAD(char, game_options_showScore)
    SREAD(char, game_options_bopIcons)
    SREAD(char, game_options_vsync)
    SREAD(char, game_options_fillScreen)
    SREAD(char, game_options_botplay)
    SREAD(char, game_options_ghost)
    SREAD(char, game_options_scroll)
    SREAD(char, game_options_showFps)
    SREAD(int,  game_options_scheme) 
    SREAD(int,  game_options_fps) 
    SREAD(float,game_options_zoomFactorUI) 
    SREAD(float,game_options_zoomFactorGAME) 

    GameData_Checks();
}

void GameData_Flush(void) {  
    rewind(saveFile);

    char version = SAVE_VERSION;
    SWRITE(char, version)
    SWRITE(char, game_options_showScore)
    SWRITE(char, game_options_bopIcons)
    SWRITE(char, game_options_vsync)
    SWRITE(char, game_options_fillScreen)
    SWRITE(char, game_options_botplay)
    SWRITE(char, game_options_ghost)
    SWRITE(char, game_options_scroll)
    SWRITE(char, game_options_showFps)
    SWRITE(int,  game_options_scheme) 
    SWRITE(int,  game_options_fps)
    SWRITE(float,game_options_zoomFactorUI) 
    SWRITE(float,game_options_zoomFactorGAME) 
}

void GameData_Stop(void) {
    GameData_Flush();
    fclose(saveFile);
}