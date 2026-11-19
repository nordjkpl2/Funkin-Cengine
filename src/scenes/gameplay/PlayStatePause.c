/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePause.c - menu de pausa do PlayState.
 */

#include "scenes/AllScenes.h"
#include "scenes/gameplay/PlayStatePause.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "raymath.h"
#include <math.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

static char isPaused;
static int currentOption;
static float optionOffset;
static float pauseTimer;

void PlayStatePause_Reset(void) {
    isPaused = 0;
    currentOption = 0;
    optionOffset = 0;
}

void PlayStatePause_Open(void) {
    pauseTimer = -0.1f; // evita que o mesmo Enter que abriu o menu escolha uma opcao
    isPaused = 1;
    PlayStateAudio_Pause();
}

char PlayStatePause_IsPaused(void) {
    return isPaused;
}

PlayStatePauseAction PlayStatePause_Update(float dt) {
    pauseTimer += dt;

    PlayStatePauseAction action = PLAYSTATE_PAUSE_NONE;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        currentOption++;
        if (currentOption >= 3)
            currentOption = 0;
    }
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        currentOption--;
        if (currentOption < 0)
            currentOption = 2;
    }
    else if (pauseTimer > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        if (currentOption == 0)
            isPaused = 0;
        else if (currentOption == 1)
            action = PLAYSTATE_PAUSE_RESTART;
        else
            action = PLAYSTATE_PAUSE_EXIT;
    }

    optionOffset = Lerp(optionOffset, currentOption, dt * 10.0f);
    return action;
}

void PlayStatePause_Draw(char exitToEditor) {
    const char* const options[3] = {"Resume", "Restart", exitToEditor ? "Exit to Chart Editor" : "Exit to Menu"};

    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color) {0, 0, 0, 100});

    Color color = WHITE;
    for (int i = 0; i < 3; i++) {
        const int fontSize = 85;
        Vector2 pos = {150.0f + fabsf(i - optionOffset) * -30, SCREEN_H / 2 + (i - optionOffset) * (15 + fontSize)};
        color.a = currentOption == i ? 255 : 150;
        DrawTextEx(mainFont, options[i], pos, fontSize, 5, color);
    }
}
