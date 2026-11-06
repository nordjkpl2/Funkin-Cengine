/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "GameData.h" 
#include "Log.h"
#include <stdio.h>

#define IERR Log_Fatal("[ SAVE ] invalid data");
#define READ(type, count, output) if(fread(&output, sizeof(type), count, saveFile) != count) IERR
#define WRITE(type, count, source) if(fwrite(&source, sizeof(type), count, saveFile) != count) Log_Fatal("[ SAVE ] unable to write to save file!");

#define SREAD(type, output) READ(type, 1, output)
#define SWRITE(type, source) WRITE(type, 1, source)

GameOptions gameOptions;

const char* game_available_schemes[SCHEME_COUNT] = {"Arrows", "WASD", "DFJK", "ASKL", "Custom"};
int game_scheme_keys[SCHEME_COUNT][4] = {
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT},
    {KEY_A, KEY_S, KEY_W, KEY_D},
    {KEY_D, KEY_F, KEY_J, KEY_K},
    {KEY_A, KEY_S, KEY_K, KEY_L},
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT}   /* Custom: comeca igual ao Arrows */
};

static FILE* saveFile;

#define SAVE_VERSION 6   /* 6: + hitSound e as 4 teclas do esquema Custom */

static void GameData_Defaults(void) {
    game_scheme_keys[SCHEME_CUSTOM][0] = KEY_LEFT;
    game_scheme_keys[SCHEME_CUSTOM][1] = KEY_DOWN;
    game_scheme_keys[SCHEME_CUSTOM][2] = KEY_UP;
    game_scheme_keys[SCHEME_CUSTOM][3] = KEY_RIGHT;
    gameOptions = (GameOptions) {
        .showScore = 1, .bopIcons = 1, .showFps = 1,
        .fps = 120,
        .zoomFactorUI = 1.0f, .zoomFactorGAME = 1.0f,
    };
}

static void GameData_Checks(void) {
    if(gameOptions.scheme < 0 || gameOptions.scheme >= SCHEME_COUNT) IERR
    if(gameOptions.fps < 0) IERR
    if(gameOptions.ghost < 0 || gameOptions.ghost > 2) IERR
    if(gameOptions.scroll < 0 || gameOptions.scroll > 2) IERR
    if(gameOptions.hitSound < 0 || gameOptions.hitSound > 1) IERR
    if(gameOptions.showFps < 0 || gameOptions.showFps > 1) IERR
    for(int k = 0; k < 4; k++)
        if(game_scheme_keys[SCHEME_CUSTOM][k] <= 0) IERR
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

    SREAD(char, gameOptions.showScore)
    SREAD(char, gameOptions.bopIcons)
    SREAD(char, gameOptions.vsync)
    SREAD(char, gameOptions.fillScreen)
    SREAD(char, gameOptions.botplay)
    SREAD(char, gameOptions.ghost)
    SREAD(char, gameOptions.scroll)
    SREAD(char, gameOptions.showFps)
    SREAD(char, gameOptions.hitSound)
    SREAD(int,  gameOptions.scheme) 
    SREAD(int,  gameOptions.fps) 
    SREAD(float,gameOptions.zoomFactorUI) 
    SREAD(float,gameOptions.zoomFactorGAME) 
    READ(int, 4, game_scheme_keys[SCHEME_CUSTOM][0])

    GameData_Checks();
}

void GameData_Flush(void) {  
    rewind(saveFile);

    char version = SAVE_VERSION;
    SWRITE(char, version)
    SWRITE(char, gameOptions.showScore)
    SWRITE(char, gameOptions.bopIcons)
    SWRITE(char, gameOptions.vsync)
    SWRITE(char, gameOptions.fillScreen)
    SWRITE(char, gameOptions.botplay)
    SWRITE(char, gameOptions.ghost)
    SWRITE(char, gameOptions.scroll)
    SWRITE(char, gameOptions.showFps)
    SWRITE(char, gameOptions.hitSound)
    SWRITE(int,  gameOptions.scheme) 
    SWRITE(int,  gameOptions.fps)
    SWRITE(float,gameOptions.zoomFactorUI) 
    SWRITE(float,gameOptions.zoomFactorGAME) 
    WRITE(int, 4, game_scheme_keys[SCHEME_CUSTOM][0])
}

void GameData_Stop(void) {
    GameData_Flush();
    fclose(saveFile);
}