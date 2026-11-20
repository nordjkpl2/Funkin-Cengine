/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePause.c - menu de pausa do PlayState.
 */

#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayStatePause.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "raymath.h"
#include "rendering/TextMeasure.h"
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

/* Opcoes: 0 Resume, 1 Skip time, 2 Restart, 3 Exit. */
#define OPTION_COUNT 4
#define OPTION_SKIP 1

/* Segurar LEFT/RIGHT repete o seek: espera SEEK_REPEAT_DELAY e depois repete a cada SEEK_REPEAT_INTERVAL. */
#define SEEK_REPEAT_DELAY 0.4f
#define SEEK_REPEAT_INTERVAL 0.08f

typedef struct {
    float held;   /* tempo segurando a tecla */
    float acc;    /* acumulador do intervalo de repeticao */
} SeekHold;

static SeekHold seekHoldLeft, seekHoldRight;

/* Retorna 1 quando a tecla deve disparar um seek neste frame (aperto inicial ou repeticao). */
static char SeekFires(SeekHold* h, int key, float dt) {
    if (IsKeyPressed(key)) {
        h->held = 0.0f;
        h->acc = 0.0f;
        return 1;
    }
    if (!IsKeyDown(key)) {
        h->held = 0.0f;
        h->acc = 0.0f;
        return 0;
    }

    h->held += dt;
    if (h->held < SEEK_REPEAT_DELAY) return 0;

    h->acc += dt;
    if (h->acc >= SEEK_REPEAT_INTERVAL) {
        h->acc -= SEEK_REPEAT_INTERVAL;
        return 1;
    }
    return 0;
}

PlayStatePauseAction PlayStatePause_Update(float dt) {
    pauseTimer += dt;

    PlayStatePauseAction action = PLAYSTATE_PAUSE_NONE;

    /* Avalia os dois lados sempre, para o estado de segurar acompanhar a tecla. */
    const char seekBack = SeekFires(&seekHoldLeft, KEY_LEFT, dt);
    const char seekForward = SeekFires(&seekHoldRight, KEY_RIGHT, dt);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        currentOption++;
        if (currentOption >= OPTION_COUNT)
            currentOption = 0;
    }
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        currentOption--;
        if (currentOption < 0)
            currentOption = OPTION_COUNT - 1;
    }
    else if (currentOption == OPTION_SKIP && seekBack) {
        action = PLAYSTATE_PAUSE_SEEK_BACK;
    }
    else if (currentOption == OPTION_SKIP && seekForward) {
        action = PLAYSTATE_PAUSE_SEEK_FORWARD;
    }
    else if (pauseTimer > 0 && currentOption != OPTION_SKIP && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        if (currentOption == 0)
            isPaused = 0;
        else if (currentOption == 2)
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

/* mm:ss (segundos negativos viram 00:00). */
static void FormatClock(char out[12], float secs) {
    if (!(secs > 0.0f)) secs = 0.0f;
    const int total = (int)secs;
    snprintf(out, 12, "%02d:%02d", total / 60, total % 60);
}

void PlayStatePause_Draw(char exitToEditor, float currentSecs, float totalSecs) {
    char cur[12], tot[12], skipLabel[48];
    FormatClock(cur, currentSecs);
    FormatClock(tot, totalSecs);
    snprintf(skipLabel, sizeof(skipLabel), "Skip time  %s / %s", cur, tot);

    const char* const options[OPTION_COUNT] = {"Resume", skipLabel, "Restart", exitToEditor ? "Exit to Chart Editor" : "Exit to Menu"};
    const int fontSize = 85;
    static TextMeasure optionMeasure[OPTION_COUNT]; // so refaz a medida quando o rotulo muda (ex.: o tempo do Skip)

    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color) {0, 0, 0, 100});

    Color color = WHITE;
    for (int i = 0; i < OPTION_COUNT; i++) {
        Vector2 pos = {150.0f + fabsf(i - optionOffset) * -30, SCREEN_H / 2 + (i - optionOffset) * (15 + fontSize)};
        color.a = currentOption == i ? 255 : 150;
        DrawTextEx(mainFont, options[i], pos, fontSize, 5, color);

        if (i <= OPTION_SKIP) continue; // Resume e Skip time: sem icone

        // Icone logo depois do texto, centralizado na linha.
        const Vector2 textSize = TextMeasure_Get(&optionMeasure[i], mainFont, options[i], fontSize, 5);
        const float x0 = pos.x + textSize.x + ICON_GAP;
        const float cy = pos.y + fontSize * 0.5f;

        if (i == 2) DrawRestartIcon(x0, cy, color);
        else DrawExitIcon(x0, cy, color);
    }
}
