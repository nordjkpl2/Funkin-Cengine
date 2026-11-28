/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePause.c - menu de pausa do PlayState.
 *
 * Lista a esquerda (Resume, Skip time, Restart, Exit, ">"), cabecalho com musica e
 * dificuldade a direita e o painel Playback (Song speed) que entra pela direita.
 * Animacoes param sozinhas quando chegam no alvo; enquanto andam, pedem redesenho.
 */

#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/flow/PlayStatePause.h"
#include "scenes/gameplay/audio/PlayStateAudio.h"
#include "rendering/TextMeasure.h"
#include "GameData.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

/* Menu principal */
enum { OPT_RESUME, OPT_SKIP, OPT_RESTART, OPT_EXIT, OPT_MORE, OPTION_COUNT };
/* Painel Playback */
enum { SUB_SPEED, SUB_RESET, SUB_BACK, SUB_COUNT };

#define SPEED_STEP 0.05f

/* Segurar LEFT/RIGHT repete: espera REPEAT_DELAY e depois repete a cada REPEAT_INTERVAL. */
#define REPEAT_DELAY 0.4f
#define REPEAT_INTERVAL 0.08f

typedef struct {
    float held;   /* tempo segurando a tecla */
    float acc;    /* acumulador do intervalo de repeticao */
} KeyHold;

static char isPaused;
static char panelOpen;
static char redraw;
static int current, subCurrent;
static float cursor, subCursor;   /* posicao animada do marcador, em linhas */
static float fadeIn;              /* 0..1 ao abrir */
static float panel;               /* 0..1 painel Playback */
static float pauseTimer;
static float wheelAcc;
static KeyHold holdLeft, holdRight;

static char title[96] = "Unknown Song";
static char diffLabel[24] = "NORMAL";

void PlayStatePause_Reset(void) {
    isPaused = 0;
    panelOpen = 0;
    current = subCurrent = 0;
    cursor = subCursor = 0.0f;
    panel = 0.0f;
}

void PlayStatePause_SetInfo(const char* songDir, const char* diff) {
    // Ultima pasta de songDir, "song-name" -> "Song Name".
    const char* end = songDir + strlen(songDir);
    while (end > songDir && (end[-1] == '/' || end[-1] == '\\')) end--;
    const char* start = end;
    while (start > songDir && start[-1] != '/' && start[-1] != '\\') start--;

    size_t n = (size_t)(end - start);
    if (n == 0) n = (size_t)snprintf(title, sizeof(title), "Unknown Song");
    else {
        if (n >= sizeof(title)) n = sizeof(title) - 1;
        memcpy(title, start, n);
        title[n] = '\0';
    }
    char wordStart = 1;
    for (char* c = title; *c; c++) {
        if (*c == '-' || *c == '_') *c = ' ';
        if (wordStart) *c = (char)toupper((unsigned char)*c);
        wordStart = (*c == ' ');
    }

    snprintf(diffLabel, sizeof(diffLabel), "%s", (diff != NULL && diff[0] != 0) ? diff : "normal");
    for (char* c = diffLabel; *c; c++) *c = (char)toupper((unsigned char)*c);
}

void PlayStatePause_Open(void) {
    pauseTimer = -0.1f; // evita que o mesmo Enter que abriu o menu escolha uma opcao
    isPaused = 1;
    panelOpen = 0;
    panel = 0.0f;
    fadeIn = 0.0f;
    cursor = (float)current;
    redraw = 1;
    PlayStateAudio_Pause();
}

char PlayStatePause_IsPaused(void) {
    return isPaused;
}

char PlayStatePause_TakeRedraw(void) {
    const char r = redraw;
    redraw = 0;
    return r;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/* 1 quando a tecla dispara neste frame (aperto inicial ou repeticao). */
static char KeyFires(KeyHold* h, int key, float dt) {
    if (IsKeyPressed(key)) {
        h->held = h->acc = 0.0f;
        return 1;
    }
    if (!IsKeyDown(key)) {
        h->held = h->acc = 0.0f;
        return 0;
    }
    h->held += dt;
    if (h->held < REPEAT_DELAY) return 0;
    h->acc += dt;
    if (h->acc < REPEAT_INTERVAL) return 0;
    h->acc -= REPEAT_INTERVAL;
    return 1;
}

/* Roda do mouse em linhas inteiras (touchpad manda fracoes). Positivo = para cima. */
static int WheelSteps(void) {
    wheelAcc += GetMouseWheelMove();
    int steps = 0;
    while (wheelAcc >= 1.0f) { wheelAcc -= 1.0f; steps++; }
    while (wheelAcc <= -1.0f) { wheelAcc += 1.0f; steps--; }
    return steps;
}

typedef struct {
    int move;      /* +1 desce, -1 sobe */
    char left, right, accept, back;
} PauseInput;

static int Wrap(int v, int count) {
    v %= count;
    return v < 0 ? v + count : v;
}

static PlayStatePauseAction SetSpeed(float speed) {
    const float before = PlayStateAudio_Speed();
    PlayStateAudio_SetSpeed(roundf(speed / SPEED_STEP) * SPEED_STEP); // passo exato, sem acumular erro
    return PlayStateAudio_Speed() != before ? PLAYSTATE_PAUSE_SPEED : PLAYSTATE_PAUSE_NONE;
}

static PlayStatePauseAction UpdateMain(const PauseInput* in) {
    if (in->move != 0) {
        current = Wrap(current + in->move, OPTION_COUNT);
        return PLAYSTATE_PAUSE_NONE;
    }
    if (current == OPT_SKIP && in->left) return PLAYSTATE_PAUSE_SEEK_BACK;
    if (current == OPT_SKIP && in->right) return PLAYSTATE_PAUSE_SEEK_FORWARD;
    if (in->back) {
        isPaused = 0;
        return PLAYSTATE_PAUSE_NONE;
    }
    if (current == OPT_MORE && (in->accept || in->right)) {
        panelOpen = 1;
        subCurrent = 0;
        subCursor = 0.0f;
        return PLAYSTATE_PAUSE_NONE;
    }
    if (!in->accept) return PLAYSTATE_PAUSE_NONE;

    switch (current) {
        case OPT_RESUME: isPaused = 0; break;
        case OPT_RESTART: return PLAYSTATE_PAUSE_RESTART;
        case OPT_EXIT: return PLAYSTATE_PAUSE_EXIT;
        default: break; // Skip time: so setas
    }
    return PLAYSTATE_PAUSE_NONE;
}

static PlayStatePauseAction UpdatePanel(const PauseInput* in) {
    if (in->move != 0) {
        subCurrent = Wrap(subCurrent + in->move, SUB_COUNT);
        return PLAYSTATE_PAUSE_NONE;
    }
    if (subCurrent == SUB_SPEED && (in->left || in->right))
        return SetSpeed(PlayStateAudio_Speed() + (in->right ? SPEED_STEP : -SPEED_STEP));
    if (in->back || in->left || (in->accept && subCurrent == SUB_BACK)) {
        panelOpen = 0;
        return PLAYSTATE_PAUSE_NONE;
    }
    if (in->accept && subCurrent == SUB_RESET) return SetSpeed(1.0f);
    return PLAYSTATE_PAUSE_NONE;
}

/* Aproxima v do alvo; 1 enquanto ainda anda (para pedir redesenho). */
static char Ease(float* v, float target, float k) {
    const float d = target - *v;
    if (d == 0.0f) return 0;
    if (fabsf(d) < 0.002f) *v = target;
    else *v += d * fminf(k, 1.0f);
    return 1;
}

PlayStatePauseAction PlayStatePause_Update(float dt) {
    pauseTimer += dt;

    PauseInput in = {0};
    // Avalia os dois lados sempre, para o estado de segurar acompanhar a tecla.
    in.left = KeyFires(&holdLeft, KEY_LEFT, dt);
    in.right = KeyFires(&holdRight, KEY_RIGHT, dt);
    in.move = (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) - (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) - WheelSteps();
    in.accept = pauseTimer > 0.0f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
    in.back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);

    const int before = current * 16 + subCurrent * 2 + panelOpen;
    const PlayStatePauseAction action = panelOpen ? UpdatePanel(&in) : UpdateMain(&in);
    if (action != PLAYSTATE_PAUSE_NONE || before != current * 16 + subCurrent * 2 + panelOpen || !isPaused)
        redraw = 1;
    // Segurando LEFT/RIGHT: quadros reais (no idle o dt do loop fica parado e a repeticao erraria o ritmo).
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT))
        redraw = 1;

    const float k = dt * 22.0f;
    redraw |= Ease(&cursor, (float)current, k);
    redraw |= Ease(&subCursor, (float)subCurrent, k);
    redraw |= Ease(&fadeIn, 1.0f, dt * 18.0f);
    redraw |= Ease(&panel, panelOpen ? 1.0f : 0.0f, dt * 16.0f);
    return action;
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

#define LIST_X 104.0f
#define ROW_H 78.0f
#define FONT_ROW 56.0f
#define FONT_SMALL 30.0f
#define FONT_HINT 22.0f
#define SPACING 3.0f
#define PANEL_W 520.0f

static Color Alpha(Color c, float a) {
    c.a = (unsigned char)(fminf(fmaxf(a, 0.0f), 1.0f) * 255.0f);
    return c;
}

static float Measure(TextMeasure* m, const char* text, float size) {
    return TextMeasure_Get(m, mainFont, text, size, SPACING).x;
}

static void Text(const char* text, float x, float y, float size, Color c) {
    DrawTextEx(mainFont, text, (Vector2) {x, y}, size, SPACING, c);
}

/* Ponta de seta: duas linhas saindo de `tip` para tras, na direcao oposta a `dir` (normalizada). */
static void DrawArrowHead(Vector2 tip, Vector2 dir, float size, float stroke, Color color) {
    const Vector2 n = {-dir.y, dir.x};
    const Vector2 back = {tip.x - dir.x * size, tip.y - dir.y * size};
    DrawLineEx(tip, (Vector2) {back.x + n.x * size * 0.6f, back.y + n.y * size * 0.6f}, stroke, color);
    DrawLineEx(tip, (Vector2) {back.x - n.x * size * 0.6f, back.y - n.y * size * 0.6f}, stroke, color);
    DrawCircleV(tip, stroke * 0.5f, color);
}

#define ICON 34.0f
#define ICON_STROKE 5.0f

/* Circulo com setinha = Restart. x0 = canto esquerdo, cy = centro. */
static void DrawRestartIcon(float x0, float cy, Color color) {
    const float r = ICON * 0.5f - ICON_STROKE * 0.5f;
    const Vector2 c = {x0 + ICON * 0.5f, cy};
    const float rad = 330.0f * DEG2RAD;
    DrawRing(c, r - ICON_STROKE * 0.5f, r + ICON_STROKE * 0.5f, 30.0f, 330.0f, 28, color);
    const Vector2 dir = {-sinf(rad), cosf(rad)}; // tangente no sentido do arco
    const Vector2 tip = {c.x + r * cosf(rad) + dir.x * 4.0f, c.y + r * sinf(rad) + dir.y * 4.0f};
    DrawArrowHead(tip, dir, ICON * 0.42f, ICON_STROKE, color);
}

/* Porta com seta saindo = Exit. */
static void DrawExitIcon(float x0, float cy, Color color) {
    const float doorW = ICON * 0.55f;
    DrawRectangleLinesEx((Rectangle) {x0, cy - ICON * 0.5f, doorW, ICON}, ICON_STROKE, color);
    DrawLineEx((Vector2) {x0 + doorW * 0.6f, cy}, (Vector2) {x0 + ICON, cy}, ICON_STROKE, color);
    DrawArrowHead((Vector2) {x0 + ICON + 2.0f, cy}, (Vector2) {1.0f, 0.0f}, ICON * 0.42f, ICON_STROKE, color);
}

/* ">" desenhado com linhas (a fonte nao precisa ter o glifo bonito). */
static void DrawChevron(float x, float cy, float size, Color color) {
    DrawArrowHead((Vector2) {x + size, cy}, (Vector2) {1.0f, 0.0f}, size, ICON_STROKE, color);
}

/* mm:ss (segundos negativos viram 00:00). */
static void FormatClock(char out[12], float secs) {
    if (!(secs > 0.0f)) secs = 0.0f;
    const int total = (int)secs;
    snprintf(out, 12, "%02d:%02d", total / 60, total % 60);
}

/* Barra fina com preenchimento 0..1. */
static void DrawBar(float x, float y, float w, float t, float a) {
    DrawRectangleRec((Rectangle) {x, y, w, 4.0f}, Alpha(WHITE, 0.18f * a));
    DrawRectangleRec((Rectangle) {x, y, w * fminf(fmaxf(t, 0.0f), 1.0f), 4.0f}, Alpha(WHITE, 0.85f * a));
}

/* Marcador da linha selecionada (posicao animada). */
static void DrawCursor(float x, float y0, float pos, float a) {
    DrawRectangleRounded((Rectangle) {x, y0 + pos * ROW_H + 12.0f, 6.0f, FONT_ROW - 16.0f}, 1.0f, 4, Alpha(WHITE, a));
}

static float Selection(int row, float pos) {
    return fmaxf(0.0f, 1.0f - fabsf((float)row - pos));
}

static void DrawHeader(float a) {
    static TextMeasure mTitle, mSub;
    char sub[64];
    int n = snprintf(sub, sizeof(sub), "%s", diffLabel);
    if (PlayStateAudio_Speed() != 1.0f) n += snprintf(sub + n, sizeof(sub) - (size_t)n, "  |  %.2fx", PlayStateAudio_Speed());
    if (gameOptions.botplay) snprintf(sub + n, sizeof(sub) - (size_t)n, "  |  BOTPLAY");

    const float right = SCREEN_W - 56.0f;
    float size = 48.0f;
    float w = Measure(&mTitle, title, size);
    if (w > 640.0f) { // titulo longo: diminui em vez de cortar
        size *= 640.0f / w;
        w = 640.0f;
    }
    const float slide = (1.0f - a) * 30.0f;
    Text(title, right - w + slide, 52.0f, size, Alpha(WHITE, a));
    Text(sub, right - Measure(&mSub, sub, 26.0f) + slide, 108.0f, 26.0f, Alpha(WHITE, 0.6f * a));
}

static void DrawMainList(char exitToEditor, float currentSecs, float totalSecs, float a) {
    static TextMeasure mRow[OPTION_COUNT];
    const char* const labels[OPTION_COUNT] = {"Resume", "Skip time", "Restart", exitToEditor ? "Exit to Chart Editor" : "Exit to Menu", "Playback"};
    const float y0 = (SCREEN_H - OPTION_COUNT * ROW_H) * 0.5f + 24.0f;
    const float dim = 1.0f - 0.55f * panel; // painel aberto apaga a lista
    const float slide = (1.0f - a) * -40.0f;

    Text("PAUSED", LIST_X, y0 - 58.0f, FONT_SMALL, Alpha(WHITE, 0.45f * a * dim));
    DrawCursor(LIST_X - 34.0f + slide, y0, cursor, a * dim);

    for (int i = 0; i < OPTION_COUNT; i++) {
        const float sel = Selection(i, cursor);
        const float x = LIST_X + sel * 16.0f + slide;
        const float y = y0 + (float)i * ROW_H;
        const float cy = y + FONT_ROW * 0.5f;
        const Color c = Alpha(WHITE, (0.42f + 0.58f * sel) * a * dim);

        if (i == OPT_MORE) { // ">" no fim: abre o painel escondido
            DrawChevron(x, cy, 16.0f, c);
            char more[32] = "Playback";
            if (PlayStateAudio_Speed() != 1.0f) snprintf(more, sizeof(more), "Playback  %.2fx", PlayStateAudio_Speed());
            Text(more, x + 34.0f, cy - FONT_SMALL * 0.5f, FONT_SMALL, c);
            continue;
        }

        Text(labels[i], x, y, FONT_ROW, c);
        const float endX = x + Measure(&mRow[i], labels[i], FONT_ROW) + 28.0f;

        if (i == OPT_SKIP) {
            static TextMeasure mClock;
            char cur[12], tot[12], clock[40];
            FormatClock(cur, currentSecs);
            FormatClock(tot, totalSecs);
            snprintf(clock, sizeof(clock), "<  %s / %s  >", cur, tot);
            const float cw = Measure(&mClock, clock, FONT_SMALL);
            Text(clock, endX, cy - FONT_SMALL * 0.5f - 4.0f, FONT_SMALL, c);
            DrawBar(endX, cy + FONT_SMALL * 0.5f + 2.0f, cw, totalSecs > 0.0f ? currentSecs / totalSecs : 0.0f, (0.4f + 0.6f * sel) * a * dim);
        }
        else if (i == OPT_RESTART) DrawRestartIcon(endX, cy, c);
        else if (i == OPT_EXIT) DrawExitIcon(endX, cy, c);
    }

    Text("ENTER select    ESC resume    LEFT/RIGHT skip", LIST_X, SCREEN_H - 56.0f, FONT_HINT, Alpha(WHITE, 0.35f * a * dim));
}

static void DrawPanel(float a) {
    const float t = 1.0f - (1.0f - panel) * (1.0f - panel); // ease-out
    const float px = SCREEN_W - PANEL_W * t;
    const float pa = panel * a;
    const float x = px + 64.0f;
    const float y0 = 236.0f;
    const float speed = PlayStateAudio_Speed();

    DrawRectangleRec((Rectangle) {px, 0.0f, PANEL_W, SCREEN_H}, (Color) {12, 12, 20, (unsigned char)(235.0f * pa)});
    DrawRectangleRec((Rectangle) {px, 0.0f, 2.0f, SCREEN_H}, Alpha(WHITE, 0.25f * pa));
    Text("PLAYBACK", x, y0 - 70.0f, FONT_SMALL, Alpha(WHITE, 0.45f * pa));
    DrawCursor(x - 34.0f, y0, subCursor, pa);

    static TextMeasure mVal;
    char val[24];
    snprintf(val, sizeof(val), "<  %.2fx  >", speed);
    const float right = SCREEN_W - 48.0f;

    for (int i = 0; i < SUB_COUNT; i++) {
        const float sel = Selection(i, subCursor);
        const float y = y0 + (float)i * ROW_H;
        const Color c = Alpha(WHITE, (0.42f + 0.58f * sel) * pa);
        switch (i) {
            case SUB_SPEED: {
                Text("Speed", x + sel * 12.0f, y, 44.0f, c);
                Text(val, right - Measure(&mVal, val, 34.0f), y + 6.0f, 34.0f, c);
                // 0.5x..2.0x com marca em 1x
                const float bx = x, bw = right - x;
                const float k = (speed - PSA_SPEED_MIN) / (PSA_SPEED_MAX - PSA_SPEED_MIN);
                const float one = (1.0f - PSA_SPEED_MIN) / (PSA_SPEED_MAX - PSA_SPEED_MIN);
                DrawBar(bx, y + 56.0f, bw, k, (0.4f + 0.6f * sel) * pa);
                DrawRectangleRec((Rectangle) {bx + bw * one - 1.0f, y + 51.0f, 2.0f, 14.0f}, Alpha(WHITE, 0.5f * pa));
                DrawCircleV((Vector2) {bx + bw * k, y + 58.0f}, 7.0f, c);
            } break;
            case SUB_RESET: Text("Reset to 1.00x", x + sel * 12.0f, y + 14.0f, 38.0f, c); break;
            default: Text("Back", x + sel * 12.0f, y + 14.0f, 38.0f, c); break;
        }
    }

    Text("Pitch preserved", x, y0 + SUB_COUNT * ROW_H + 18.0f, FONT_HINT, Alpha(WHITE, 0.4f * pa));
    Text("LEFT/RIGHT change    ESC back", x, SCREEN_H - 56.0f, FONT_HINT, Alpha(WHITE, 0.35f * pa));
}

void PlayStatePause_Draw(char exitToEditor, float currentSecs, float totalSecs) {
    const float a = fadeIn;
    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, Alpha(BLACK, 0.45f * a));
    DrawRectangleGradientH(0, 0, 760, (int)SCREEN_H, Alpha(BLACK, 0.6f * a), Alpha(BLACK, 0.0f));

    DrawHeader(a * (1.0f - panel)); // o painel cobre o canto direito
    DrawMainList(exitToEditor, currentSecs, totalSecs, a);
    if (panel > 0.001f) DrawPanel(a);
}
