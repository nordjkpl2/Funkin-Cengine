/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuScreens.c - telas pretas de captura de teclas (esquema Custom) e de numero de threads.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "Game.h"
#include "GameData.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"
#include <stdio.h>


static const char* const LANE_NAMES[4] = {"LEFT", "DOWN", "UP", "RIGHT"};

bool capturing;           // tela preta de captura aberta
static int captureStep;          // 0..3 = esperando a tecla dessa direcao; 4 = confirmar
static int captureKeys[4];
static const char* captureMessage = "";

/* Nome curto da tecla para mostrar na tela. */
static void KeyName(int key, char* out, size_t size) {
    static const struct { int key; const char* name; } named[] = {
        {KEY_SPACE, "SPACE"}, {KEY_LEFT, "LEFT"}, {KEY_RIGHT, "RIGHT"}, {KEY_UP, "UP"}, {KEY_DOWN, "DOWN"},
        {KEY_COMMA, ","}, {KEY_PERIOD, "."}, {KEY_SEMICOLON, ";"}, {KEY_APOSTROPHE, "'"}, {KEY_SLASH, "/"},
        {KEY_BACKSLASH, "\\"}, {KEY_LEFT_BRACKET, "["}, {KEY_RIGHT_BRACKET, "]"}, {KEY_MINUS, "-"},
        {KEY_EQUAL, "="}, {KEY_GRAVE, "`"}, {KEY_TAB, "TAB"}, {KEY_LEFT_SHIFT, "L-SHIFT"},
        {KEY_RIGHT_SHIFT, "R-SHIFT"}, {KEY_LEFT_CONTROL, "L-CTRL"}, {KEY_RIGHT_CONTROL, "R-CTRL"},
        {KEY_LEFT_ALT, "L-ALT"}, {KEY_RIGHT_ALT, "R-ALT"}, {KEY_INSERT, "INSERT"}, {KEY_DELETE, "DELETE"},
        {KEY_HOME, "HOME"}, {KEY_END, "END"}, {KEY_PAGE_UP, "PGUP"}, {KEY_PAGE_DOWN, "PGDN"},
    };

    if (key >= KEY_A && key <= KEY_Z)         (void)snprintf(out, size, "%c", 'A' + (key - KEY_A));
    else if (key >= KEY_ZERO && key <= KEY_NINE) (void)snprintf(out, size, "%c", '0' + (key - KEY_ZERO));
    else if (key >= KEY_KP_0 && key <= KEY_KP_9) (void)snprintf(out, size, "NUM %d", key - KEY_KP_0);
    else if (key >= KEY_F1 && key <= KEY_F12)    (void)snprintf(out, size, "F%d", key - KEY_F1 + 1);
    else {
        for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++)
            if (named[i].key == key) { (void)snprintf(out, size, "%s", named[i].name); return; }
        (void)snprintf(out, size, "KEY %d", key);
    }
}

void Capture_Begin(void) {
    capturing = true;
    captureStep = 0;
    captureMessage = "";
    for (int k = 0; k < 4; k++) captureKeys[k] = 0;
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
}

void Capture_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE)) {   // cancela e mantem as teclas antigas
        capturing = false;
        return;
    }

    if (captureStep == 4) {
        if (IsKeyPressed(KEY_ENTER)) {
            for (int k = 0; k < 4; k++) game_scheme_keys[SCHEME_CUSTOM][k] = captureKeys[k];
            gameOptions.scheme = SCHEME_CUSTOM;
            options[OPTION_KEYBOARD_SCHEME].value = SCHEME_CUSTOM;
            options[OPTION_KEYBOARD_SCHEME].dirty = true;
            GameData_Flush();
            capturing = false;
        }
        else if (IsKeyPressed(KEY_BACKSPACE)) {
            captureStep = 3;
            captureMessage = "";
        }
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {   // volta uma direcao
        if (captureStep > 0) captureStep--;
        captureKeys[captureStep] = 0;
        captureMessage = "";
        return;
    }

    const int key = GetKeyPressed();
    if (key == 0) return;
    if (key == KEY_ENTER || key == KEY_KP_ENTER) { captureMessage = "ENTER e reservado"; return; }

    for (int k = 0; k < captureStep; k++) {
        if (captureKeys[k] == key) { captureMessage = "Tecla ja usada"; return; }
    }

    captureKeys[captureStep++] = key;
    captureMessage = "";
}

void Capture_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, BLACK);
    DrawTextEx(mainFont, "CUSTOM KEYS", (Vector2) {80, 50}, 70, TEXT_SPACING, YELLOW);

    for (int k = 0; k < 4; k++) {
        char name[24] = "...";
        if (captureKeys[k] != 0 && k < captureStep) KeyName(captureKeys[k], name, sizeof(name));

        char line[64];
        (void)snprintf(line, sizeof(line), "%-6s  %s", LANE_NAMES[k], name);
        Color color = k < captureStep ? GREEN : (k == captureStep ? WHITE : GRAY);
        DrawTextEx(mainFont, line, (Vector2) {120, 190.0f + (float)k * 80.0f}, 56, TEXT_SPACING, color);
    }

    if (captureStep < 4) {
        char prompt[48];
        (void)snprintf(prompt, sizeof(prompt), "Press the key for %s", LANE_NAMES[captureStep]);
        DrawTextEx(mainFont, prompt, (Vector2) {80, 540}, 36, 3, WHITE);
        DrawTextEx(mainFont, "BACKSPACE back     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    } else {
        DrawTextEx(mainFont, "ENTER to confirm", (Vector2) {80, 540}, 36, 3, GREEN);
        DrawTextEx(mainFont, "BACKSPACE back     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    }
    if (captureMessage[0] != 0)
        DrawTextEx(mainFont, captureMessage, (Vector2) {80, 595}, 28, 2, RED);
}

/* ------------------------------------------------------------------ */
/* Tela do numero de threads (modo "Limited")                          */
/* ------------------------------------------------------------------ */

bool threadsScreen;   // tela preta aberta
static int threadsInput;     // numero sendo digitado (0 = vazio)

static int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void ThreadsScreen_Begin(void) {
    threadsScreen = true;
    threadsInput = limitedThreads;
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
}

void ThreadsScreen_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE)) {   // cancela e mantem o numero antigo
        threadsScreen = false;
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        const int n = ClampInt(threadsInput, 1, THREADS_MAX);
        limitedThreads = n;
        gameOptions.threadLimit = n;
        Jobs_SetThreads(n);
        options[OPTION_THREADS].dirty = true;
        GameData_Flush();
        threadsScreen = false;
        return;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_RIGHT)) threadsInput = ClampInt(threadsInput + 1, 1, THREADS_MAX);
    else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT)) threadsInput = ClampInt(threadsInput - 1, 1, THREADS_MAX);
    else if (IsKeyPressed(KEY_BACKSPACE)) threadsInput /= 10;

    for (int d = 0; d <= 9; d++) {
        if (IsKeyPressed(KEY_ZERO + d) || IsKeyPressed(KEY_KP_0 + d)) {
            threadsInput = threadsInput * 10 + d;
            if (threadsInput > THREADS_MAX) threadsInput = d;   // 3o digito recomeca
        }
    }
}

void ThreadsScreen_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, BLACK);
    DrawTextEx(mainFont, "THREADS", (Vector2) {80, 50}, 70, TEXT_SPACING, YELLOW);

    char line[64];
    (void)snprintf(line, sizeof(line), "CPU threads: %d     Auto: %d", Thread_CoreCount(), AutoThreads());
    DrawTextEx(mainFont, line, (Vector2) {120, 190}, 40, 3, GRAY);

    if (threadsInput > 0) (void)snprintf(line, sizeof(line), "Limit: %d", threadsInput);
    else (void)snprintf(line, sizeof(line), "Limit: _");
    DrawTextEx(mainFont, line, (Vector2) {120, 290}, 80, TEXT_SPACING, WHITE);

    DrawTextEx(mainFont, "Type a number (1-64)  or  UP / DOWN", (Vector2) {80, 480}, 32, 3, WHITE);
    DrawTextEx(mainFont, "ENTER confirm", (Vector2) {80, 540}, 36, 3, threadsInput > 0 ? GREEN : GRAY);
    DrawTextEx(mainFont, "BACKSPACE erase     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
}
