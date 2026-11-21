/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "GameData.h" 
#include "Log.h"
#include <stdio.h>
#include <string.h>

#define IERR Log_Fatal("[ SAVE ] invalid data");
#define READ(type, count, output) if(fread(&output, sizeof(type), count, saveFile) != count) IERR
#define WRITE(type, count, source) if(fwrite(&source, sizeof(type), count, saveFile) != count) Log_Fatal("[ SAVE ] unable to write to save file!");

#define SREAD(type, output) READ(type, 1, output)
#define SWRITE(type, source) WRITE(type, 1, source)

GameOptions gameOptions;
char gameDisabledMods[MOD_DISABLED_MAX][MOD_NAME_MAX];
int  gameDisabledModCount;

const char* game_available_schemes[SCHEME_COUNT] = {"Arrows", "WASD", "DFJK", "ASKL", "Custom"};
int game_scheme_keys[SCHEME_COUNT][4] = {
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT},
    {KEY_A, KEY_S, KEY_W, KEY_D},
    {KEY_D, KEY_F, KEY_J, KEY_K},
    {KEY_A, KEY_S, KEY_K, KEY_L},
    {KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT}   /* Custom: comeca igual ao Arrows */
};

static FILE* saveFile;

#define SAVE_VERSION 17  /* 17: + streamingJsonParse (1 byte, no fim); 16: + loseIcon (1 byte, no fim); 15: + horizontalMenu (1 byte, no fim); hiddenNotesAlt aceita 0..3; 14: + hiddenNotesAlt (1 byte, no fim); 13: + laneTint (12 bytes, no fim); 12: + strumBgPercent e strumMobileVisible (no fim); 11: + menuMusic (no fim, apos as mods); 10: + healthDrainPlay e healthDrainBotplay (apos scorePosition); 9: + scorePosition (apos threadLimit); 8: + lista de mods desativados; 7: + threadLimit (no fim); 6: + hitSound e as 4 teclas do esquema Custom. v7 e v8 ainda sao lidos. */

static void GameData_Defaults(void) {
    game_scheme_keys[SCHEME_CUSTOM][0] = KEY_LEFT;
    game_scheme_keys[SCHEME_CUSTOM][1] = KEY_DOWN;
    game_scheme_keys[SCHEME_CUSTOM][2] = KEY_UP;
    game_scheme_keys[SCHEME_CUSTOM][3] = KEY_RIGHT;
    gameDisabledModCount = 0;
    gameOptions = (GameOptions) {
        .showScore = 1, .bopIcons = 1, .showFps = 1,
        .scorePosition = SCORE_POS_PSYCH, .strumBgPercent = 0,
        .fps = 120,
        .zoomFactorUI = 1.0f, .zoomFactorGAME = 1.0f,
        .laneTint = {{255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}},
    };
}

/* Hidden Notes: a lado (BF ou oponente) aparece? bfSide = 1 para BF; singerBf = quem canta na secao.
   0 Off: tudo. 1 Alternate: so quem canta. 2 Only Playable: so BF. 3 Alternate Playable: BF sempre + oponente quando ele canta. */
char GameData_HiddenShowsSide(char bfSide, char singerBf) {
    switch (gameOptions.hiddenNotesAlt) {
        case 1:  return (bfSide != 0) == (singerBf != 0);
        case 2:  return bfSide != 0;
        case 3:  return bfSide != 0 || !singerBf;
        default: return 1;
    }
}

Color GameData_LaneTint(int lane) {
    if (lane < 0 || lane > 3) return WHITE;
    const unsigned char* c = gameOptions.laneTint[lane];
    return (Color) {c[0], c[1], c[2], 255};
}

static void GameData_Checks(void) {
    if(gameOptions.scheme < 0 || gameOptions.scheme >= SCHEME_COUNT) IERR
    if(gameOptions.fps < 0) IERR
    if(gameOptions.threadLimit < 0 || gameOptions.threadLimit > 64) IERR
    if(gameDisabledModCount < 0 || gameDisabledModCount > MOD_DISABLED_MAX) IERR
    if(gameOptions.ghost < 0 || gameOptions.ghost > 2) IERR
    if(gameOptions.scroll < 0 || gameOptions.scroll > 2) IERR
    if(gameOptions.hitSound < 0 || gameOptions.hitSound > 1) IERR
    if(gameOptions.showFps < 0 || gameOptions.showFps > 1) IERR
    if(gameOptions.scorePosition < SCORE_POS_CENGINE || gameOptions.scorePosition > SCORE_POS_PSYCH) IERR
    if(gameOptions.strumBgPercent < 0 || gameOptions.strumBgPercent > 100) IERR
    if(gameOptions.strumMobileVisible < 0 || gameOptions.strumMobileVisible > 1) IERR
    if(gameOptions.hiddenNotesAlt < 0 || gameOptions.hiddenNotesAlt > 3) IERR
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

    if(version < 7 || version > SAVE_VERSION) {
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
    SREAD(int,  gameOptions.threadLimit)
    gameOptions.scorePosition = SCORE_POS_CENGINE;   /* saves antigos (< v9) */
    if(version >= 9) SREAD(char, gameOptions.scorePosition)
    gameOptions.healthDrainPlay = gameOptions.healthDrainBotplay = 0;   /* saves antigos (< v10) */
    if(version >= 10) {
        SREAD(char, gameOptions.healthDrainPlay)
        SREAD(char, gameOptions.healthDrainBotplay)
    }
    gameDisabledModCount = 0;
    if(version >= 8) {
        SREAD(int, gameDisabledModCount)
        if(gameDisabledModCount < 0 || gameDisabledModCount > MOD_DISABLED_MAX) IERR
        READ(char, gameDisabledModCount * MOD_NAME_MAX, gameDisabledMods[0][0])
        for(int i = 0; i < gameDisabledModCount; i++) gameDisabledMods[i][MOD_NAME_MAX - 1] = 0;
    }
    gameOptions.menuMusic[0] = 0;   /* saves antigos (< v11): padrao */
    if(version >= 11) {
        READ(char, MENU_MUSIC_NAME_MAX, gameOptions.menuMusic[0])
        gameOptions.menuMusic[MENU_MUSIC_NAME_MAX - 1] = 0;
    }
    gameOptions.strumBgPercent = 0;      /* saves antigos (< v12): padrao */
    gameOptions.strumMobileVisible = 0;
    if(version >= 12) {
        SREAD(char, gameOptions.strumBgPercent)
        SREAD(char, gameOptions.strumMobileVisible)
    }
    memset(gameOptions.laneTint, 255, sizeof(gameOptions.laneTint));   /* saves antigos (< v13): padrao */
    if(version >= 13)
        READ(unsigned char, sizeof(gameOptions.laneTint), gameOptions.laneTint[0][0])
    gameOptions.hiddenNotesAlt = 0;      /* saves antigos (< v14): padrao */
    if(version >= 14)
        SREAD(char, gameOptions.hiddenNotesAlt)
    gameOptions.horizontalMenu = 0;      /* saves antigos (< v15): padrao */
    if(version >= 15)
        SREAD(char, gameOptions.horizontalMenu)
    gameOptions.loseIcon = 0;            /* saves antigos (< v16): padrao */
    if(version >= 16)
        SREAD(char, gameOptions.loseIcon)
    gameOptions.streamingJsonParse = 0;  /* saves antigos (< v17): padrao off */
    if(version >= 17)
        SREAD(char, gameOptions.streamingJsonParse)

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
    SWRITE(int,  gameOptions.threadLimit)
    SWRITE(char, gameOptions.scorePosition)
    SWRITE(char, gameOptions.healthDrainPlay)
    SWRITE(char, gameOptions.healthDrainBotplay)
    SWRITE(int,  gameDisabledModCount)
    WRITE(char, gameDisabledModCount * MOD_NAME_MAX, gameDisabledMods[0][0])
    WRITE(char, MENU_MUSIC_NAME_MAX, gameOptions.menuMusic[0])
    SWRITE(char, gameOptions.strumBgPercent)
    SWRITE(char, gameOptions.strumMobileVisible)
    WRITE(unsigned char, sizeof(gameOptions.laneTint), gameOptions.laneTint[0][0])
    SWRITE(char, gameOptions.hiddenNotesAlt)
    SWRITE(char, gameOptions.horizontalMenu)
    SWRITE(char, gameOptions.loseIcon)
    SWRITE(char, gameOptions.streamingJsonParse)
}

void GameData_Stop(void) {
    GameData_Flush();
    fclose(saveFile);
}