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

#define ICON_SIZE 56.0f
#define ICON_GAP 40.0f
#define ICON_STROKE 7.0f

/* Ponta de seta: duas linhas saindo de `tip` para tras, na direcao oposta a `dir` (normalizada). */
static void DrawArrowHead(Vector2 tip, Vector2 dir, float size, Color color) {
    const Vector2 n = {-dir.y, dir.x};
    const Vector2 back = {tip.x - dir.x * size, tip.y - dir.y * size};
    const Vector2 a = {back.x + n.x * size * 0.6f, back.y + n.y * size * 0.6f};
    const Vector2 b = {back.x - n.x * size * 0.6f, back.y - n.y * size * 0.6f};
    DrawLineEx(tip, a, ICON_STROKE, color);
    DrawLineEx(tip, b, ICON_STROKE, color);
    DrawCircleV(tip, ICON_STROKE * 0.5f, color);
}

/* Circulo com setinha = Restart. (cx, cy) e o centro do icone, x0 o canto esquerdo. */
static void DrawRestartIcon(float x0, float cy, Color color) {
    const float r = ICON_SIZE * 0.5f - ICON_STROKE * 0.5f;
    const Vector2 c = {x0 + ICON_SIZE * 0.5f, cy};
    const float endDeg = 330.0f;
    const float rad = endDeg * DEG2RAD;

    DrawRing(c, r - ICON_STROKE * 0.5f, r + ICON_STROKE * 0.5f, 30.0f, endDeg, 32, color);

    const Vector2 p = {c.x + r * cosf(rad), c.y + r * sinf(rad)};
    const Vector2 dir = {-sinf(rad), cosf(rad)}; // tangente no sentido do arco
    const Vector2 tip = {p.x + dir.x * 6.0f, p.y + dir.y * 6.0f};
    DrawArrowHead(tip, dir, ICON_SIZE * 0.45f, color);
}

/* Porta com seta saindo = Exit. */
static void DrawExitIcon(float x0, float cy, Color color) {
    const float half = ICON_SIZE * 0.5f;
    const float doorW = ICON_SIZE * 0.55f;

    DrawRectangleLinesEx((Rectangle) {x0, cy - half, doorW, ICON_SIZE}, ICON_STROKE, color);
    DrawCircleV((Vector2) {x0 + doorW * 0.75f, cy}, ICON_STROKE * 0.6f, color); // maçaneta

    const float ax0 = x0 + doorW * 0.6f;
    const float ax1 = x0 + ICON_SIZE;
    DrawLineEx((Vector2) {ax0, cy}, (Vector2) {ax1, cy}, ICON_STROKE, color);
    DrawArrowHead((Vector2) {ax1 + 2.0f, cy}, (Vector2) {1.0f, 0.0f}, ICON_SIZE * 0.45f, color);
}

void PlayStatePause_Draw(char exitToEditor) {
    const char* const options[3] = {"Resume", "Restart", exitToEditor ? "Exit to Chart Editor" : "Exit to Menu"};
    const int fontSize = 85;

    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color) {0, 0, 0, 100});

    Color color = WHITE;
    for (int i = 0; i < 3; i++) {
        Vector2 pos = {150.0f + fabsf(i - optionOffset) * -30, SCREEN_H / 2 + (i - optionOffset) * (15 + fontSize)};
        color.a = currentOption == i ? 255 : 150;
        DrawTextEx(mainFont, options[i], pos, fontSize, 5, color);

        if (i == 0) continue; // Resume: sem icone

        // Icone logo depois do texto, centralizado na linha.
        const Vector2 textSize = MeasureTextEx(mainFont, options[i], fontSize, 5);
        const float x0 = pos.x + textSize.x + ICON_GAP;
        const float cy = pos.y + fontSize * 0.5f;

        if (i == 1) DrawRestartIcon(x0, cy, color);
        else DrawExitIcon(x0, cy, color);
    }
}
