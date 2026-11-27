/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * HudEditor.c - HUD Editor: preview 1:1 do HUD do gameplay. Cada elemento (barra de vida,
 * tempo, score, titulo, botplay) pode ser arrastado, escalado e escondido; grava em assets/hud.json
 * (HudConfig). O gameplay le o layout no Create da cena.
 *
 * Desenho em poucos draw calls: formas usam o retangulo branco do atlas da fonte (Modding_BatchWith),
 * entao preview, textos e painel sao um batch cada (so os icones trocam de textura).
 */
#include "scenes/modding/hudeditor/HudEditor.h"
#include "scenes/modding/ModdingMenuInternal.h"
#include "scenes/api/FontLoader.h"
#include "data/config/HudConfig.h"
#include "characters/Character.h"
#include "rendering/TextMeasure.h"
#include "GameData.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define UNDO_CAP 48
#define SNAP_DIST 8.0f
#define STEP_SCALE 5
#define PANEL_W 310.0f
#define PANEL_Y 50.0f
#define PANEL_H 640.0f
#define PANEL_MARGIN 20.0f
#define HOLD_DELAY 0.35f  /* botao -/+ segurado: espera e repete */
#define HOLD_RATE 0.04f

/* Medidas do gameplay (PlayStateHealth.c, PlayStateText.c, PlayStateStrums.c): manter iguais. */
#define BAR_W 600.0f
#define BAR_H 17.0f
#define BAR_PAD 4.0f
#define BAR_Y_DOWN 103.0f
#define BAR_Y_UP 600.0f
#define TXT_CENTER_X 640.0f
#define TXT_RIGHT_X 1270.0f
#define TXT_LEFT_X 10.0f
#define STACK_LAST_Y 690.0f
#define STACK_LINE_H 18.0f
#define TIME_TOP_Y 15.0f
#define TIME_LOW_Y 690.0f
#define TIME_RIGHTDOWN_Y 640.0f
#define TIME_FONT 20.0f
#define TIME_BAR_W 394.0f
#define TIME_BAR_H 13.0f
#define TIME_BAR_BORDER 3.0f
#define SCORE_FONT 16.0f
#define PSYCH_FONT 15.0f
#define PSYCH_OFFSET 30.0f
#define TOP_SCORE_Y 15.0f
#define TOP_SCORE_UNDER_TIME 44.0f
#define TITLE_FONT 13.0f
#define BOT_FONT 22.0f
#define LANE_SPACING 110.0f
#define STRUM_SIZE 104.0f

/* Botoes do painel: mesmo id no hit (Update) e no desenho. */
enum {
    BTN_EL0 = 0,                     /* linha do elemento i: BTN_EL0 + i */
    BTN_EYE0 = BTN_EL0 + HUD_ELEMENTS, /* liga/desliga o elemento i */
    BTN_XM = BTN_EYE0 + HUD_ELEMENTS, BTN_XP,
    BTN_YM, BTN_YP,
    BTN_SM, BTN_SP,
    BTN_WM, BTN_WP,
    BTN_VIS, BTN_SCROLL,
    BTN_SAVE, BTN_UNDO, BTN_RESET,
    BTN_COUNT
};

static HudConfig draft, saved;
static HudConfig undoStack[UNDO_CAP], redoStack[UNDO_CAP];
static int undoN, redoN;
static int sel, hover = -1, hoverBtn = -1, heldBtn = -1;
static float holdT;
static char dragging, dragMoved, snapX;
static Vector2 dragGrab;
static float dragBaseCX;   /* centro x do elemento sem deslocamento (para o snap) */
static char previewDown, panelShown, confirmExit;
static float panelX, panelAlpha;
static char status[128];
static float statusT;
static Font vcr;
static Texture2D iconDad, iconBf;
static int cursorShown;

/* Geometria do frame (preview + caixas). Refeita no fim do Update. */
typedef struct {
    Rectangle bar, dadIcon, bfIcon, timeBar;
    char timeBarOn;
    const char *timeText;
    Vector2 timePos;
    float timeSize;
    int scoreLines;
    const char *scoreText[3];
    Vector2 scorePos[3];
    float scoreSize;
    Vector2 titlePos, botPos;
    float titleSize, botSize;
    Rectangle box[HUD_ELEMENTS];
} HudLayout;

static HudLayout L;
/* medidas em cache: 0 tempo, 1-3 linhas do score, 4 titulo, 5 botplay, 6 toast, 7-10 valores do painel, 11 tag */
static TextMeasure measure[12];

static const Color ACCENT = {42, 209, 86, 255};
static const Color MUTED = {150, 150, 160, 255};
static const Color DAD_COLOR = {204, 85, 180, 255};
static const Color BF_COLOR = {0, 217, 204, 255};

/* ------------------------------------------------------------------------- */
/* Util                                                                      */
/* ------------------------------------------------------------------------- */

static int ClampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static Rectangle Union(Rectangle a, Rectangle b)
{
    const float x0 = fminf(a.x, b.x), y0 = fminf(a.y, b.y);
    const float x1 = fmaxf(a.x + a.width, b.x + b.width), y1 = fmaxf(a.y + a.height, b.y + b.height);
    return (Rectangle){x0, y0, x1 - x0, y1 - y0};
}

static Rectangle Pad(Rectangle r, float p)
{
    return (Rectangle){r.x - p, r.y - p, r.width + 2.0f * p, r.height + 2.0f * p};
}

static Color Alpha(Color c, float k)
{
    c.a = (unsigned char)((float)c.a * k);
    return c;
}

static char Same(const HudConfig *a, const HudConfig *b)
{
    if (a->barWidthPct != b->barWidthPct) return 0;
    for (int i = 0; i < HUD_ELEMENTS; ++i) {
        const HudElement *x = &a->el[i], *y = &b->el[i];
        if (x->offsetX != y->offsetX || x->offsetY != y->offsetY ||
            x->scalePct != y->scalePct || (x->visible != 0) != (y->visible != 0))
            return 0;
    }
    return 1;
}

static void SetStatus(const char *msg)
{
    snprintf(status, sizeof(status), "%s", msg);
    statusT = 2.5f;
}

static Vector2 Measure(int slot, Font f, const char *text, float size)
{
    return TextMeasure_Get(&measure[slot], f, text, size, 1);
}

static char ShiftDown(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static char CtrlDown(void) { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

/* ------------------------------------------------------------------------- */
/* Undo                                                                      */
/* ------------------------------------------------------------------------- */

/* Guarda o estado antes de uma edicao. */
static void Hud_Push(void)
{
    if (undoN == UNDO_CAP) {
        memmove(undoStack, undoStack + 1, sizeof(undoStack[0]) * (UNDO_CAP - 1));
        --undoN;
    }
    undoStack[undoN++] = draft;
    redoN = 0;
    confirmExit = 0;
}

static void Hud_Undo(void)
{
    if (undoN == 0) { SetStatus("Nothing to undo"); return; }
    if (redoN < UNDO_CAP) redoStack[redoN++] = draft;
    draft = undoStack[--undoN];
    confirmExit = 0;
}

static void Hud_Redo(void)
{
    if (redoN == 0) return;
    if (undoN < UNDO_CAP) undoStack[undoN++] = draft;
    draft = redoStack[--redoN];
    confirmExit = 0;
}

/* ------------------------------------------------------------------------- */
/* Layout (espelha o gameplay com as opcoes atuais)                          */
/* ------------------------------------------------------------------------- */

static void Hud_Layout(void)
{
    const HudElement *e = draft.el;

    /* barra de vida + icones (vida em 50%) */
    {
        const HudElement *h = &e[HUD_EL_HEALTH];
        const float s = (float)h->scalePct / 100.0f;
        const float w = roundf(BAR_W * (float)draft.barWidthPct / 100.0f);
        L.bar = (Rectangle){TXT_CENTER_X - w * 0.5f + (float)h->offsetX,
                            (previewDown ? BAR_Y_DOWN : BAR_Y_UP) + (float)h->offsetY, w, BAR_H};
        const float iconX = L.bar.x + BAR_PAD + (w - 2.0f * BAR_PAD) * 0.5f;
        const float cy = L.bar.y + BAR_H * 0.5f;
        L.dadIcon = (Rectangle){iconX - 40.0f * s - 75.0f * s, cy - 75.0f * s, 130.0f * s, 130.0f * s};
        L.bfIcon = (Rectangle){iconX + 40.0f * s - 75.0f * s, cy - 75.0f * s, 145.0f * s, 145.0f * s};
        L.box[HUD_EL_HEALTH] = Pad(Union(L.bar, Union(L.dadIcon, L.bfIcon)), 2.0f);
    }

    /* tempo */
    {
        const HudElement *t = &e[HUD_EL_TIME];
        const float ts = (float)t->scalePct / 100.0f;
        const char lift = gameOptions.showScore && gameOptions.showAccuracy && gameOptions.scorePosition == SCORE_POS_CENGINE;
        char right = 0;
        float x = TXT_CENTER_X, y = TIME_TOP_Y;

        switch (gameOptions.timePosition) {
            case TIME_POS_LOW:       y = TIME_LOW_Y; break;
            case TIME_POS_RIGHTUP:   right = 1; x = TXT_RIGHT_X; break;
            case TIME_POS_RIGHTDOWN: right = 1; x = TXT_RIGHT_X; y = TIME_RIGHTDOWN_Y - (float)lift * STACK_LINE_H; break;
            default: break;
        }
        if (gameOptions.timeText == TIME_TEXT_LEFT)         L.timeText = "2:22";
        else if (gameOptions.timeText == TIME_TEXT_ELAPSED) L.timeText = "1:23";
        else                                                L.timeText = "1:23 / 3:45";
        L.timeSize = TIME_FONT * ts;

        L.timeBarOn = gameOptions.timeStyle == TIME_STYLE_BAR;
        if (L.timeBarOn) {
            const float bw = TIME_BAR_W * ts, bh = TIME_BAR_H * ts, b = TIME_BAR_BORDER * ts;
            const float bx = roundf(right ? x - b - bw : x - bw * 0.5f);
            L.timeBar = (Rectangle){bx + (float)t->offsetX, y + b + (float)t->offsetY, bw, bh};
            right = 0;
            x = bx + bw * 0.5f;
            y = y + b + (TIME_BAR_H - TIME_FONT) * ts * 0.5f;
        }
        const Vector2 m = Measure(0, vcr, L.timeText, L.timeSize);
        L.timePos = (Vector2){(right ? x - m.x : x - m.x * 0.5f) + (float)t->offsetX, y + (float)t->offsetY};
        L.box[HUD_EL_TIME] = (Rectangle){L.timePos.x, L.timePos.y, m.x, fmaxf(m.y, L.timeSize)};
        if (L.timeBarOn)
            L.box[HUD_EL_TIME] = Union(L.box[HUD_EL_TIME], Pad(L.timeBar, TIME_BAR_BORDER * ts));
        L.box[HUD_EL_TIME] = Pad(L.box[HUD_EL_TIME], 4.0f);
    }

    /* score */
    {
        const HudElement *sc = &e[HUD_EL_SCORE];
        const float ss = (float)sc->scalePct / 100.0f;
        const char acc = gameOptions.showAccuracy;
        const int pos = gameOptions.scorePosition;

        if (pos == SCORE_POS_PSYCH || pos == SCORE_POS_TOP) {
            L.scoreLines = 1;
            L.scoreText[0] = acc ? "Score: 123450 | Misses: 3 | Accuracy: 98.50%" : "Score: 123450 | Misses: 3";
            L.scoreSize = PSYCH_FONT * ss;
            float y = L.bar.y + PSYCH_OFFSET; /* segue a barra de vida, como no jogo */
            if (pos == SCORE_POS_TOP)
                y = gameOptions.timePosition == TIME_POS_DEFAULT ? TOP_SCORE_UNDER_TIME : TOP_SCORE_Y;
            const Vector2 m = Measure(1, vcr, L.scoreText[0], L.scoreSize);
            L.scorePos[0] = (Vector2){TXT_CENTER_X - m.x * 0.5f + (float)sc->offsetX, y + (float)sc->offsetY};
            L.box[HUD_EL_SCORE] = (Rectangle){L.scorePos[0].x, L.scorePos[0].y, m.x, fmaxf(m.y, L.scoreSize)};
        } else {
            static const char *const lines[3] = {"Score: 123450", "Misses: 3", "Accuracy: 98.50%"};
            const char left = pos == SCORE_POS_LEFT;
            const float lineH = STACK_LINE_H * ss;
            const float y0 = STACK_LAST_Y + STACK_LINE_H - (acc ? 3.0f : 2.0f) * lineH;
            L.scoreLines = acc ? 3 : 2;
            L.scoreSize = SCORE_FONT * ss;
            for (int i = 0; i < L.scoreLines; ++i) {
                const Vector2 m = Measure(1 + i, vcr, lines[i], L.scoreSize);
                const float x = left ? TXT_LEFT_X : TXT_RIGHT_X - m.x;
                const Rectangle r = {x + (float)sc->offsetX, y0 + lineH * (float)i + (float)sc->offsetY, m.x, fmaxf(m.y, L.scoreSize)};
                L.scoreText[i] = lines[i];
                L.scorePos[i] = (Vector2){r.x, r.y};
                L.box[HUD_EL_SCORE] = i ? Union(L.box[HUD_EL_SCORE], r) : r;
            }
        }
        L.box[HUD_EL_SCORE] = Pad(L.box[HUD_EL_SCORE], 4.0f);
    }

    /* titulo e botplay (alinhados a esquerda) */
    {
        const HudElement *ti = &e[HUD_EL_TITLE], *bo = &e[HUD_EL_BOTPLAY];
        L.titleSize = TITLE_FONT * (float)ti->scalePct / 100.0f;
        L.botSize = BOT_FONT * (float)bo->scalePct / 100.0f;
        L.titlePos = (Vector2){22.0f + (float)ti->offsetX, 46.0f + (float)ti->offsetY};
        L.botPos = (Vector2){22.0f + (float)bo->offsetX, 20.0f + (float)bo->offsetY};
        const Vector2 mt = Measure(4, vcr, "Song Name", L.titleSize);
        const Vector2 mb = Measure(5, vcr, "BOTPLAY", L.botSize);
        L.box[HUD_EL_TITLE] = Pad((Rectangle){L.titlePos.x, L.titlePos.y, mt.x, fmaxf(mt.y, L.titleSize)}, 4.0f);
        L.box[HUD_EL_BOTPLAY] = Pad((Rectangle){L.botPos.x, L.botPos.y, mb.x, fmaxf(mb.y, L.botSize)}, 4.0f);
    }
}

/* ------------------------------------------------------------------------- */
/* Painel                                                                    */
/* ------------------------------------------------------------------------- */

static Rectangle Hud_PanelRect(void) { return (Rectangle){panelX, PANEL_Y, PANEL_W, PANEL_H}; }

#define ROW_EL_Y 74.0f
#define ROW_EL_H 32.0f
#define ROW_PROP_Y 292.0f
#define ROW_PROP_H 36.0f
#define ROW_TOGGLE_Y 444.0f
#define ROW_BTN_Y 488.0f

static Rectangle Hud_Button(int id)
{
    const float x = panelX, y = PANEL_Y, w = PANEL_W;

    if (id < BTN_EYE0)
        return (Rectangle){x + 10.0f, y + ROW_EL_Y + ROW_EL_H * (float)(id - BTN_EL0), w - 20.0f, ROW_EL_H - 2.0f};
    if (id < BTN_XM)
        return (Rectangle){x + w - 64.0f, y + ROW_EL_Y + ROW_EL_H * (float)(id - BTN_EYE0) + 4.0f, 48.0f, ROW_EL_H - 10.0f};
    if (id <= BTN_WP) {
        const int row = (id - BTN_XM) / 2;
        const char plus = (char)((id - BTN_XM) & 1);
        return (Rectangle){x + w - (plus ? 52.0f : 168.0f), y + ROW_PROP_Y + ROW_PROP_H * (float)row, 36.0f, ROW_PROP_H - 6.0f};
    }
    if (id == BTN_VIS || id == BTN_SCROLL) {
        const float bw = (w - 50.0f) * 0.5f;
        return (Rectangle){x + 20.0f + (id == BTN_SCROLL ? bw + 10.0f : 0.0f), y + ROW_TOGGLE_Y, bw, 32.0f};
    }
    {
        const float bw = (w - 60.0f) / 3.0f;
        return (Rectangle){x + 20.0f + (bw + 10.0f) * (float)(id - BTN_SAVE), y + ROW_BTN_Y, bw, 34.0f};
    }
}

static char Hud_ButtonEnabled(int id)
{
    if (id == BTN_WM || id == BTN_WP) return sel == HUD_EL_HEALTH;
    if (id == BTN_UNDO) return undoN > 0;
    return 1;
}

static int Hud_ButtonAt(Vector2 m)
{
    /* o olho fica dentro da linha do elemento: testa antes dela */
    for (int i = 0; i < HUD_ELEMENTS; ++i)
        if (CheckCollisionPointRec(m, Hud_Button(BTN_EYE0 + i))) return BTN_EYE0 + i;
    for (int id = 0; id < BTN_COUNT; ++id) {
        if (id >= BTN_EYE0 && id < BTN_XM) continue;
        if (Hud_ButtonEnabled(id) && CheckCollisionPointRec(m, Hud_Button(id))) return id;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Edicao                                                                    */
/* ------------------------------------------------------------------------- */

static void Hud_ResetElement(int i)
{
    HudConfig d;
    HudConfig_Defaults(&d);
    draft.el[i] = d.el[i];
    if (i == HUD_EL_HEALTH) draft.barWidthPct = d.barWidthPct;
}

static void Hud_Save(void)
{
    HudConfig_Set(&draft);
    if (HudConfig_Save()) {
        saved = draft;
        confirmExit = 0;
        SetStatus("Saved assets/hud.json");
    } else {
        SetStatus("Could not write assets/hud.json");
    }
}

/* Acao de botao (fresh = clique novo, nao repeticao do segurar). */
static void Hud_Press(int id, char fresh)
{
    HudElement *e = &draft.el[sel];
    const int step = ShiftDown() ? 10 : 1;

    if (fresh && id >= BTN_XM && id <= BTN_VIS) Hud_Push();
    switch (id) {
        case BTN_XM: e->offsetX = ClampI(e->offsetX - step, -HUD_OFFSET_MAX, HUD_OFFSET_MAX); break;
        case BTN_XP: e->offsetX = ClampI(e->offsetX + step, -HUD_OFFSET_MAX, HUD_OFFSET_MAX); break;
        case BTN_YM: e->offsetY = ClampI(e->offsetY - step, -HUD_OFFSET_MAX, HUD_OFFSET_MAX); break;
        case BTN_YP: e->offsetY = ClampI(e->offsetY + step, -HUD_OFFSET_MAX, HUD_OFFSET_MAX); break;
        case BTN_SM: e->scalePct = ClampI(e->scalePct - STEP_SCALE, HUD_SCALE_MIN, HUD_SCALE_MAX); break;
        case BTN_SP: e->scalePct = ClampI(e->scalePct + STEP_SCALE, HUD_SCALE_MIN, HUD_SCALE_MAX); break;
        case BTN_WM: draft.barWidthPct = ClampI(draft.barWidthPct - STEP_SCALE, HUD_WIDTH_MIN, HUD_WIDTH_MAX); break;
        case BTN_WP: draft.barWidthPct = ClampI(draft.barWidthPct + STEP_SCALE, HUD_WIDTH_MIN, HUD_WIDTH_MAX); break;
        case BTN_VIS: e->visible = !e->visible; break;
        case BTN_SCROLL: previewDown = !previewDown; break;
        case BTN_SAVE: Hud_Save(); break;
        case BTN_UNDO: Hud_Undo(); break;
        case BTN_RESET: Hud_Push(); Hud_ResetElement(sel); break;
        default:
            if (id >= BTN_EYE0 && id < BTN_XM) {
                Hud_Push();
                draft.el[id - BTN_EYE0].visible = !draft.el[id - BTN_EYE0].visible;
            } else if (id >= BTN_EL0 && id < BTN_EYE0) {
                sel = id - BTN_EL0;
            }
            break;
    }
}

static void Hud_SetCursor(int c)
{
    if (c == cursorShown) return;
    cursorShown = c;
    SetMouseCursor(c);
}

static void Hud_Keyboard(void)
{
    const char ctrl = CtrlDown(), shift = ShiftDown();
    const int step = shift ? 10 : 1;
    HudElement *e = &draft.el[sel];

    if (ctrl) {
        if (IsKeyPressed(KEY_S)) Hud_Save();
        if (IsKeyPressed(KEY_Z)) { if (shift) Hud_Redo(); else Hud_Undo(); }
        if (IsKeyPressed(KEY_Y)) Hud_Redo();
        return;
    }

    if (IsKeyPressed(KEY_TAB))
        sel = (sel + (shift ? HUD_ELEMENTS - 1 : 1)) % HUD_ELEMENTS;

    /* setas: Pressed guarda undo, Repeat so move */
    {
        static const int keys[4] = {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN};
        for (int k = 0; k < 4; ++k) {
            const char pressed = IsKeyPressed(keys[k]);
            if (!pressed && !IsKeyPressedRepeat(keys[k])) continue;
            if (pressed) Hud_Push();
            if (k < 2) e->offsetX = ClampI(e->offsetX + (k ? step : -step), -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
            else       e->offsetY = ClampI(e->offsetY + (k == 3 ? step : -step), -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
        }
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))           Hud_Press(BTN_SP, 1);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))      Hud_Press(BTN_SM, 1);
    if (sel == HUD_EL_HEALTH && IsKeyPressed(KEY_RIGHT_BRACKET))       Hud_Press(BTN_WP, 1);
    if (sel == HUD_EL_HEALTH && IsKeyPressed(KEY_LEFT_BRACKET))        Hud_Press(BTN_WM, 1);
    if (IsKeyPressed(KEY_V)) Hud_Press(BTN_VIS, 1);
    if (IsKeyPressed(KEY_D)) previewDown = !previewDown;
    if (IsKeyPressed(KEY_H)) panelShown = !panelShown;
    if (IsKeyPressed(KEY_R)) {
        Hud_Push();
        if (shift) { HudConfig_Defaults(&draft); SetStatus("All elements reset"); }
        else       Hud_ResetElement(sel);
    }
}

static void Hud_Mouse(void)
{
    const Vector2 m = RayGame_MouseWorld();
    const char inPanel = panelShown && CheckCollisionPointRec(m, Hud_PanelRect());
    HudElement *e = &draft.el[sel];

    hover = -1;
    hoverBtn = -1;

    if (dragging) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            dragging = 0;
            snapX = 0;
            Hud_SetCursor(MOUSE_CURSOR_DEFAULT);
            return;
        }
        int nx = (int)roundf(m.x - dragGrab.x);
        const int ny = (int)roundf(m.y - dragGrab.y);
        snapX = 0;
        if (!IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT) &&
            fabsf(dragBaseCX + (float)nx - TXT_CENTER_X) < SNAP_DIST) {
            nx = (int)roundf(TXT_CENTER_X - dragBaseCX);
            snapX = 1;
        }
        nx = ClampI(nx, -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
        if (nx != e->offsetX || ny != e->offsetY) {
            if (!dragMoved) { Hud_Push(); dragMoved = 1; }
            e->offsetX = nx;
            e->offsetY = ClampI(ny, -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
        }
        hover = sel;
        Hud_SetCursor(MOUSE_CURSOR_RESIZE_ALL);
        return;
    }

    if (inPanel) {
        hoverBtn = Hud_ButtonAt(m);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoverBtn >= 0) {
            Hud_Press(hoverBtn, 1);
            heldBtn = hoverBtn;
            holdT = 0.0f;
        }
        Hud_SetCursor(hoverBtn >= 0 ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    } else {
        for (int i = HUD_ELEMENTS - 1; i >= 0 && hover < 0; --i)
            if (CheckCollisionPointRec(m, L.box[i])) hover = i;

        if (hover >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            sel = hover;
            e = &draft.el[sel];
            dragging = 1;
            dragMoved = 0;
            dragGrab = (Vector2){m.x - (float)e->offsetX, m.y - (float)e->offsetY};
            dragBaseCX = L.box[sel].x + L.box[sel].width * 0.5f - (float)e->offsetX;
        }
        /* roda sobre um elemento: escala */
        {
            const float wheel = GetMouseWheelMove();
            if (hover >= 0 && wheel != 0.0f) {
                sel = hover;
                Hud_Press(wheel > 0.0f ? BTN_SP : BTN_SM, 1);
            }
        }
        Hud_SetCursor(hover >= 0 ? MOUSE_CURSOR_RESIZE_ALL : MOUSE_CURSOR_DEFAULT);
    }

    /* -/+ segurado repete */
    if (heldBtn >= 0) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || hoverBtn != heldBtn || heldBtn < BTN_XM || heldBtn > BTN_WP) {
            heldBtn = -1;
        } else {
            holdT += RayGame_DeltaTime();
            while (holdT > HOLD_DELAY) {
                Hud_Press(heldBtn, 0);
                holdT -= HOLD_RATE;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Ciclo                                                                     */
/* ------------------------------------------------------------------------- */

void Hud_Enter(void)
{
    draft = saved = *HudConfig_Get();
    undoN = redoN = 0;
    sel = HUD_EL_HEALTH;
    hover = hoverBtn = heldBtn = -1;
    dragging = dragMoved = snapX = 0;
    previewDown = SCROLL_IS_DOWN(gameOptions.scroll);
    panelShown = 1;
    confirmExit = 0;
    status[0] = '\0';
    statusT = 0.0f;
    cursorShown = MOUSE_CURSOR_DEFAULT;

    if (vcr.texture.id == 0) {
        vcr = LoadFontEx("assets/fonts/vcr.ttf", 32, 0, 0);
        SetTextureFilter(vcr.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (iconDad.id == 0) iconDad = Render_LoadTexture("assets/images/icons/dad.png");
    if (iconBf.id == 0)  iconBf = Render_LoadTexture("assets/images/icons/bf.png");

    Hud_Layout();
    panelX = 1280.0f - PANEL_W - PANEL_MARGIN;
    panelAlpha = 1.0f;
}

void Hud_Leave(void)
{
    if (vcr.texture.id != 0) UnloadFont(vcr);
    if (iconDad.id != 0) UnloadTexture(iconDad);
    if (iconBf.id != 0) UnloadTexture(iconBf);
    vcr = (Font){0};
    iconDad = iconBf = (Texture2D){0};
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

char Hud_Update(void)
{
    const float dt = RayGame_DeltaTime();

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dragging) {
            /* cancela o arraste em andamento */
            if (dragMoved) Hud_Undo();
            dragging = 0;
            snapX = 0;
        } else if (!Same(&draft, &saved) && !confirmExit) {
            confirmExit = 1;
            SetStatus("Unsaved changes: ESC again to discard, Ctrl+S to save");
        } else {
            return 1;
        }
    }

    if (!dragging) Hud_Keyboard();
    Hud_Mouse();
    Hud_Layout();

    if (statusT > 0.0f) statusT -= dt;

    /* painel vai para o lado oposto ao elemento selecionado; some um pouco durante o arraste */
    {
        const Rectangle b = L.box[sel];
        const float target = (b.x + b.width * 0.5f < TXT_CENTER_X) ? 1280.0f - PANEL_W - PANEL_MARGIN : PANEL_MARGIN;
        float k = dt * 12.0f;
        if (k > 1.0f) k = 1.0f;
        panelX += (target - panelX) * k;
        panelAlpha += ((dragging ? 0.35f : 1.0f) - panelAlpha) * k;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Desenho                                                                   */
/* ------------------------------------------------------------------------- */

/* Texto do HUD: contorno de 8 copias (tempo/score, como o raster do jogo) ou sombra (titulo/botplay). */
static void Hud_TextOutline(const char *t, Vector2 p, float size, float k)
{
    static const float dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f}};
    const Color o = Alpha(BLACK, k);
    for (int i = 0; i < 8; ++i)
        DrawTextEx(vcr, t, (Vector2){p.x + dirs[i][0], p.y + dirs[i][1]}, size, 1, o);
    DrawTextEx(vcr, t, p, size, 1, Alpha(WHITE, k));
}

static void Hud_TextShadow(const char *t, Vector2 p, float size, float sh, Color c, Color s)
{
    DrawTextEx(vcr, t, (Vector2){p.x + sh, p.y + sh}, size, 1, s);
    DrawTextEx(vcr, t, p, size, 1, c);
}

/* Fator de alfa do elemento: escondido aparece apagado (ainda da para selecionar). */
static float Hud_Vis(int i) { return draft.el[i].visible ? 1.0f : 0.3f; }

static void Hud_DrawPreview(void)
{
    /* fundo escurecido + grade */
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 170});
    for (int x = 80; x < 1280; x += 80) DrawRectangle(x, 0, 1, 720, (Color){255, 255, 255, 10});
    for (int y = 80; y < 720; y += 80)  DrawRectangle(0, y, 1280, 1, (Color){255, 255, 255, 10});
    DrawRectangle(639, 0, 2, 720, (Color){255, 255, 255, 18});

    /* receptores (so referencia, nao editaveis) */
    {
        const float sy = previewDown ? 600.0f : 50.0f;
        for (int side = 0; side < 2; ++side)
            for (int lane = 0; lane < 4; ++lane) {
                const Rectangle r = {(side ? 740.0f : 90.0f) + LANE_SPACING * (float)lane, sy, STRUM_SIZE, STRUM_SIZE};
                DrawRectangleRounded(r, 0.3f, 6, (Color){255, 255, 255, 8});
                DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2.0f, (Color){255, 255, 255, 28});
            }
    }

    /* barra de vida */
    {
        const float k = Hud_Vis(HUD_EL_HEALTH);
        const Rectangle in = {L.bar.x + BAR_PAD, L.bar.y + BAR_PAD, L.bar.width - 2.0f * BAR_PAD, L.bar.height - 2.0f * BAR_PAD};
        DrawRectangleRec(L.bar, Alpha(BLACK, k));
        DrawRectangleRec((Rectangle){in.x, in.y, in.width * 0.5f, in.height}, Alpha(DAD_COLOR, k));
        DrawRectangleRec((Rectangle){in.x + in.width * 0.5f, in.y, in.width * 0.5f, in.height}, Alpha(BF_COLOR, k));
    }

    /* barra do tempo (estilo bar) */
    if (L.timeBarOn) {
        const float k = Hud_Vis(HUD_EL_TIME);
        const float b = TIME_BAR_BORDER * (float)draft.el[HUD_EL_TIME].scalePct / 100.0f;
        DrawRectangleRec(Pad(L.timeBar, b), Alpha(BLACK, k));
        DrawRectangleRec((Rectangle){L.timeBar.x, L.timeBar.y, L.timeBar.width * 0.37f, L.timeBar.height}, Alpha(WHITE, k));
    }
}

static void Hud_DrawIcons(void)
{
    const Color c = Alpha(WHITE, Hud_Vis(HUD_EL_HEALTH));
    if (iconDad.id) DrawTexturePro(iconDad, Character_IconFrame(iconDad), L.dadIcon, (Vector2){0, 0}, 0.0f, c);
    if (iconBf.id)  DrawTexturePro(iconBf, Character_IconFrame(iconBf), L.bfIcon, (Vector2){0, 0}, 0.0f, c);
}

static void Hud_DrawTexts(void)
{
    {
        const float k = Hud_Vis(HUD_EL_TIME);
        Hud_TextOutline(L.timeText, L.timePos, L.timeSize, k);
    }
    {
        const float k = Hud_Vis(HUD_EL_SCORE);
        for (int i = 0; i < L.scoreLines; ++i)
            Hud_TextOutline(L.scoreText[i], L.scorePos[i], L.scoreSize, k);
    }
    {
        const float k = Hud_Vis(HUD_EL_TITLE);
        Hud_TextShadow("Song Name", L.titlePos, L.titleSize, 1.0f,
                       Alpha((Color){225, 225, 225, 220}, k), Alpha((Color){0, 0, 0, 180}, k));
    }
    {
        const float k = Hud_Vis(HUD_EL_BOTPLAY);
        Hud_TextShadow("BOTPLAY", L.botPos, L.botSize, 2.0f * (float)draft.el[HUD_EL_BOTPLAY].scalePct / 100.0f,
                       Alpha((Color){255, 255, 255, 125}, k), Alpha((Color){0, 0, 0, 110}, k));
    }
}

/* Caixas de hover/selecao e guia do snap (formas, mesmo batch dos textos do preview). */
static void Hud_DrawSelection(void)
{
    if (hover >= 0 && hover != sel)
        DrawRectangleRoundedLinesEx(L.box[hover], 0.15f, 4, 1.5f, (Color){255, 255, 255, 110});

    {
        const Rectangle b = L.box[sel];
        DrawRectangleRounded(b, 0.15f, 4, (Color){ACCENT.r, ACCENT.g, ACCENT.b, 28});
        DrawRectangleRoundedLinesEx(b, 0.15f, 4, 2.0f, ACCENT);
        /* alcas nos cantos */
        const float hs = 7.0f;
        const float xs[2] = {b.x - hs * 0.5f, b.x + b.width - hs * 0.5f};
        const float ys[2] = {b.y - hs * 0.5f, b.y + b.height - hs * 0.5f};
        for (int i = 0; i < 4; ++i)
            DrawRectangleRec((Rectangle){xs[i & 1], ys[i >> 1], hs, hs}, WHITE);
    }

    if (snapX && dragging)
        DrawRectangle(639, 0, 2, 720, ACCENT);
}

/* Etiqueta com nome e deslocamento acima (ou abaixo) do selecionado. Fonte da UI. */
static void Hud_DrawTag(void)
{
    char text[64];
    const HudElement *e = &draft.el[sel];
    const Rectangle b = L.box[sel];
    snprintf(text, sizeof(text), "%s  %+d, %+d", HudConfig_Name(sel), e->offsetX, e->offsetY);
    const Vector2 m = Measure(11, mainFont, text, 17);
    const float w = m.x + 16.0f, h = 24.0f;
    float x = b.x + b.width * 0.5f - w * 0.5f;
    float y = b.y - h - 8.0f;
    if (y < 4.0f) y = b.y + b.height + 8.0f;
    x = fmaxf(4.0f, fminf(x, 1276.0f - w));
    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.5f, 6, (Color){ACCENT.r, ACCENT.g, ACCENT.b, 230});
    DrawTextEx(mainFont, text, (Vector2){x + 8.0f, y + 3.0f}, 17, 1, (Color){10, 20, 12, 255});
}

/* Botao com fundo; estado visual por hover/ativo/desligado. */
static void Hud_DrawButtonBg(int id, char active, float a)
{
    const Rectangle r = Hud_Button(id);
    const char on = Hud_ButtonEnabled(id);
    Color bgc = active ? (Color){ACCENT.r, ACCENT.g, ACCENT.b, 70} : (Color){255, 255, 255, 16};
    if (on && hoverBtn == id) bgc.a = (unsigned char)(bgc.a + 34);
    if (on && heldBtn == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) bgc = (Color){ACCENT.r, ACCENT.g, ACCENT.b, 120};
    DrawRectangleRounded(r, 0.3f, 6, Alpha(bgc, a * (on ? 1.0f : 0.5f)));
    if (active) DrawRectangleRoundedLinesEx(r, 0.3f, 6, 1.5f, Alpha(ACCENT, a));
}

/* "-" ou "+" desenhado com retangulos (nitido em qualquer tamanho). */
static void Hud_DrawSign(int id, char plus, float a)
{
    const Rectangle r = Hud_Button(id);
    const float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
    const Color c = Alpha(Hud_ButtonEnabled(id) ? WHITE : MUTED, a);
    DrawRectangleRec((Rectangle){cx - 6.0f, cy - 1.0f, 12.0f, 2.0f}, c);
    if (plus) DrawRectangleRec((Rectangle){cx - 1.0f, cy - 6.0f, 2.0f, 12.0f}, c);
}

static void Hud_LabelCenter(int slot, const char *text, Rectangle r, float size, Color c)
{
    const Vector2 m = Measure(slot, mainFont, text, size);
    DrawTextEx(mainFont, text, (Vector2){r.x + (r.width - m.x) * 0.5f, r.y + (r.height - size) * 0.5f}, size, 1, c);
}

static void Hud_DrawPanel(void)
{
    const float a = panelAlpha;
    const Rectangle p = Hud_PanelRect();
    const char dirty = !Same(&draft, &saved);
    const Color text = Alpha(WHITE, a), muted = Alpha(MUTED, a);
    char buf[32];

    DrawRectangleRounded(p, 0.04f, 6, Alpha((Color){12, 12, 20, 232}, a));
    DrawRectangleRoundedLinesEx(p, 0.04f, 6, 2.0f, Alpha((Color){ACCENT.r, ACCENT.g, ACCENT.b, 130}, a));

    DrawTextEx(mainFont, "HUD EDITOR", (Vector2){p.x + 20.0f, p.y + 14.0f}, 26, 2, Alpha(ACCENT, a));
    if (dirty) {
        DrawCircle((int)(p.x + p.width - 28.0f), (int)(p.y + 28.0f), 5.0f, Alpha(YELLOW, a));
        DrawTextEx(mainFont, "unsaved", (Vector2){p.x + p.width - 100.0f, p.y + 19.0f}, 16, 1, Alpha(YELLOW, a));
    }

    DrawTextEx(mainFont, "ELEMENTS", (Vector2){p.x + 20.0f, p.y + 52.0f}, 16, 2, muted);
    for (int i = 0; i < HUD_ELEMENTS; ++i) {
        const Rectangle r = Hud_Button(BTN_EL0 + i);
        const Rectangle eye = Hud_Button(BTN_EYE0 + i);
        const char vis = draft.el[i].visible;
        if (i == sel) {
            DrawRectangleRounded(r, 0.25f, 6, Alpha((Color){ACCENT.r, ACCENT.g, ACCENT.b, 45}, a));
            DrawRectangleRec((Rectangle){r.x, r.y + 5.0f, 3.0f, r.height - 10.0f}, Alpha(ACCENT, a));
        } else if (hoverBtn == BTN_EL0 + i || hover == i) {
            DrawRectangleRounded(r, 0.25f, 6, Alpha((Color){255, 255, 255, 14}, a));
        }
        DrawTextEx(mainFont, HudConfig_Name(i), (Vector2){r.x + 12.0f, r.y + 5.0f}, 19, 1, vis ? text : muted);
        DrawRectangleRounded(eye, 0.5f, 6, vis ? Alpha((Color){ACCENT.r, ACCENT.g, ACCENT.b, hoverBtn == BTN_EYE0 + i ? 150 : 100}, a)
                                               : Alpha((Color){255, 255, 255, hoverBtn == BTN_EYE0 + i ? 40 : 18}, a));
        DrawTextEx(mainFont, vis ? "ON" : "OFF", (Vector2){eye.x + (vis ? 13.0f : 9.0f), eye.y + 2.0f}, 16, 1, vis ? text : muted);
    }

    DrawRectangle((int)p.x + 20, (int)(p.y + 246.0f), (int)p.width - 40, 1, Alpha((Color){255, 255, 255, 40}, a));
    DrawTextEx(mainFont, HudConfig_Name(sel), (Vector2){p.x + 20.0f, p.y + 256.0f}, 22, 1, text);

    {
        static const char *const labels[4] = {"X", "Y", "Scale", "Width"};
        const HudElement *e = &draft.el[sel];
        for (int row = 0; row < 4; ++row) {
            const int idM = BTN_XM + row * 2, idP = idM + 1;
            const char on = Hud_ButtonEnabled(idM);
            const Rectangle bm = Hud_Button(idM), bp = Hud_Button(idP);
            const Rectangle val = {bm.x + bm.width, bm.y, bp.x - bm.x - bm.width, bm.height};
            switch (row) {
                case 0:  snprintf(buf, sizeof(buf), "%+d", e->offsetX); break;
                case 1:  snprintf(buf, sizeof(buf), "%+d", e->offsetY); break;
                case 2:  snprintf(buf, sizeof(buf), "%d%%", e->scalePct); break;
                default: snprintf(buf, sizeof(buf), on ? "%d%%" : "-", draft.barWidthPct); break;
            }
            DrawTextEx(mainFont, labels[row], (Vector2){p.x + 20.0f, bm.y + 5.0f}, 19, 1, on ? text : Alpha(muted, 0.5f));
            Hud_DrawButtonBg(idM, 0, a);
            Hud_DrawButtonBg(idP, 0, a);
            Hud_DrawSign(idM, 0, a);
            Hud_DrawSign(idP, 1, a);
            Hud_LabelCenter(7 + row, buf, val, 19, on ? text : Alpha(muted, 0.5f));
        }
    }

    Hud_DrawButtonBg(BTN_VIS, draft.el[sel].visible, a);
    {
        const Rectangle rv = Hud_Button(BTN_VIS), rs = Hud_Button(BTN_SCROLL);
        DrawTextEx(mainFont, draft.el[sel].visible ? "Visible" : "Hidden", (Vector2){rv.x + 12.0f, rv.y + 6.0f}, 18, 1, text);
        Hud_DrawButtonBg(BTN_SCROLL, 0, a);
        DrawTextEx(mainFont, previewDown ? "Downscroll" : "Upscroll", (Vector2){rs.x + 12.0f, rs.y + 6.0f}, 18, 1, text);
    }

    {
        static const char *const names[3] = {"Save", "Undo", "Reset"};
        for (int i = 0; i < 3; ++i) {
            const int id = BTN_SAVE + i;
            const Rectangle r = Hud_Button(id);
            Hud_DrawButtonBg(id, id == BTN_SAVE && dirty, a);
            DrawTextEx(mainFont, names[i], (Vector2){r.x + 14.0f, r.y + 7.0f}, 19, 1,
                       Hud_ButtonEnabled(id) ? text : Alpha(muted, 0.6f));
        }
    }

    {
        static const char *const help[] = {
            "Drag: move   Alt: no snap",
            "Arrows: 1 px   Shift: 10 px",
            "Tab: next   +/- or wheel: scale",
            "[ ]: bar width   V: show/hide",
            "R: reset   Shift+R: reset all",
            "D: scroll   H: hide panel",
            "Ctrl+S save  Ctrl+Z undo  Esc back",
        };
        float y = p.y + ROW_BTN_Y + 46.0f;
        for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); ++i, y += 14.0f)
            DrawTextEx(mainFont, help[i], (Vector2){p.x + 20.0f, y}, 14, 1, Alpha(muted, 0.9f));
    }
}

static void Hud_DrawToast(void)
{
    const float a = statusT > 0.4f ? 1.0f : statusT / 0.4f;
    const Vector2 m = Measure(6, mainFont, status, 19);
    const Rectangle r = {640.0f - m.x * 0.5f - 18.0f, 664.0f, m.x + 36.0f, 34.0f};
    DrawRectangleRounded(r, 0.5f, 8, Alpha((Color){12, 12, 20, 235}, a));
    DrawRectangleRoundedLinesEx(r, 0.5f, 8, 1.5f, Alpha(confirmExit ? YELLOW : ACCENT, a));
    DrawTextEx(mainFont, status, (Vector2){r.x + 18.0f, r.y + 7.0f}, 19, 1, Alpha(WHITE, a));
}

void Hud_Draw(void)
{
    Modding_DrawBackground();

    /* batch 1: formas do preview (atlas do vcr) */
    Modding_BatchWith(vcr);
    Hud_DrawPreview();
    /* icones: 2 texturas proprias */
    Hud_DrawIcons();
    /* batch 2: textos do HUD + caixas (atlas do vcr) */
    Hud_DrawTexts();
    Hud_DrawSelection();

    /* batch 3: UI do editor (atlas da fonte principal) */
    Modding_BatchWith(mainFont);
    Hud_DrawTag();
    if (panelShown) Hud_DrawPanel();
    if (statusT > 0.0f && status[0]) Hud_DrawToast();

    Render_StopCamera();
}
