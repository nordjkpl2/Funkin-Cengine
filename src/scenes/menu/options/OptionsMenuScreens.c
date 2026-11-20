/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuScreens.c - telas pretas de captura de teclas (esquema Custom) e de numero de threads.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "GameData.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"
#include "rendering/TextMeasure.h"
#include <stdio.h>


static const char* const LANE_NAMES[4] = {"LEFT", "DOWN", "UP", "RIGHT"};
static TextMeasure laneNameMeasure[4]; // nomes fixos
static TextMeasure keyNameMeasure[4];  // tecla escolhida: muda so ao capturar

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
    NoteColors_ReleaseFx();   /* recarrega a previa com a skin padrao */
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

/* Tela de teclas Custom: 4 colunas com as notas reais (cor da coluna), tecla embaixo de cada uma.
   Coluna atual destacada; as ja confirmadas ficam verdes. */
#define CAP_CX0 220.0f
#define CAP_STEP 260.0f
#define CAP_CY 330.0f
#define CAP_BOX_W 200.0f
#define CAP_BOX_H 56.0f

void Capture_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, (Color) {0, 0, 0, 225});
    DrawTextEx(mainFont, "CUSTOM KEYS", (Vector2) {80, 14}, 40, TEXT_SPACING, YELLOW);

    for (int k = 0; k < 4; k++) {
        const float cx = CAP_CX0 + (float)k * CAP_STEP;
        const bool current = k == captureStep;
        const bool done = k < captureStep;

        // Nome da coluna, acima da nota.
        const Color nameColor = current ? WHITE : (done ? GREEN : GRAY);
        const Vector2 ts = TextMeasure_Get(&laneNameMeasure[k], mainFont, LANE_NAMES[k], 30, 2);
        DrawTextEx(mainFont, LANE_NAMES[k], (Vector2) {cx - ts.x * 0.5f, CAP_CY - 150.0f}, 30, 2, nameColor);

        // Nota real da skin; a coluna atual aparece maior.
        NoteColors_DrawLanePreview(k, cx, CAP_CY, current ? 0.85f : 0.6f);

        // Caixa com a tecla escolhida (ou "...").
        char name[24] = "...";
        if (captureKeys[k] != 0 && done) KeyName(captureKeys[k], name, sizeof(name));
        const Rectangle box = {cx - CAP_BOX_W * 0.5f, CAP_CY + 120.0f, CAP_BOX_W, CAP_BOX_H};
        DrawRectangleRec(box, (Color) {30, 30, 30, 255});
        DrawRectangleLinesEx(box, current ? 4.0f : 2.0f, current ? WHITE : (done ? GREEN : GRAY));
        const Vector2 ks = TextMeasure_Get(&keyNameMeasure[k], mainFont, name, 34, 2);
        DrawTextEx(mainFont, name, (Vector2) {cx - ks.x * 0.5f, box.y + (CAP_BOX_H - ks.y) * 0.5f}, 34, 2,
                   done ? WHITE : GRAY);
    }

    if (captureStep < 4) {
        char prompt[48];
        (void)snprintf(prompt, sizeof(prompt), "Press the key for %s", LANE_NAMES[captureStep]);
        DrawTextEx(mainFont, prompt, (Vector2) {80, 560}, 36, 3, WHITE);
    } else {
        DrawTextEx(mainFont, "ENTER to confirm", (Vector2) {80, 560}, 36, 3, GREEN);
    }
    if (captureMessage[0] != 0)
        DrawTextEx(mainFont, captureMessage, (Vector2) {80, 610}, 28, 2, RED);

    DrawTextEx(mainFont, "Press the key for each note, in order.",
               (Vector2) {80, 662}, 22, 1, GRAY);
    DrawTextEx(mainFont, "BACKSPACE: back     ESC: cancel",
               (Vector2) {80, 688}, 22, 1, GRAY);
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
