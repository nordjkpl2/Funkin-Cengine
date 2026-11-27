/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* ModdingUi.c - kit de interface dos editores de mod. Ver ModdingUi.h. Sem alocacao. */
#include "scenes/modding/ModdingUi.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/api/FontLoader.h"
#include "Game.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TF_PAD        8.0f
#define TF_BACKUP     512
#define STATUS_TIME   4.0f
#define CLIP_MAX      4
#define DRAG_PX       3.0f    /* pixels por passo ao arrastar um numero */
#define DBL_CLICK_S   0.35

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

static bool mouseTaken;
static bool keysTaken;
static float blink;

static char statusText[200];
static float statusTimer;
static MUiTone statusTone;

static Rectangle clipStack[CLIP_MAX];
static int clipDepth;

/* campo de texto com foco */
static struct {
    char *buf;
    size_t cap;
    unsigned flags;
    int caret, anchor;      /* bytes; selecao = [min, max) */
    float scroll;           /* rolagem horizontal do texto (px) */
    bool selecting;         /* arrastando o mouse para selecionar */
    bool seen;              /* o editor chamou o campo neste frame */
    const char *lost;       /* perdeu o foco para outro campo neste frame (recebe BLUR) */
    char backup[TF_BACKUP];
} tf;

/* campo numerico: identificado pela posicao do retangulo */
static struct {
    float x, y;
    bool down, drag, typing, seen;
    float startMouse, startValue;
    char buf[32];
} nf;

/* lista com a barra de rolagem sendo arrastada */
static const MUiList *barDrag;

/* janela */
enum { MODAL_NONE = 0, MODAL_PROMPT, MODAL_CONFIRM };
static struct {
    int kind;
    int id;
    char title[96];
    char hint[200];
    char error[200];
    char text[512];
} modal;
static bool inModal;      /* widgets da propria janela rodando */
static bool drawingModal; /* hover so aparece na janela enquanto ela esta aberta */

static bool Blocked(void)
{
    return modal.kind != MODAL_NONE && !inModal;
}

static bool HoverVisible(Rectangle r)
{
    return (modal.kind == MODAL_NONE || drawingModal) && MUi_Hover(r);
}

/* ------------------------------------------------------------------------- */
/* Ciclo                                                                     */
/* ------------------------------------------------------------------------- */

void MUi_Reset(void)
{
    memset(&tf, 0, sizeof(tf));
    memset(&nf, 0, sizeof(nf));
    memset(&modal, 0, sizeof(modal));
    barDrag = NULL;
    statusText[0] = '\0';
    statusTimer = 0.0f;
    mouseTaken = keysTaken = false;
    clipDepth = 0;
}

void MUi_FrameBegin(void)
{
    mouseTaken = false;
    keysTaken = false;
    tf.lost = NULL;

    /* Campo que o editor parou de chamar perde o foco (senao os atalhos ficariam presos). */
    if (tf.buf && !tf.seen)
        tf.buf = NULL;
    tf.seen = false;
    if ((nf.typing || nf.down) && !nf.seen)
        nf.typing = nf.down = nf.drag = false;
    nf.seen = false;
}

void MUi_Tick(float dt)
{
    blink += dt;
    if (blink > 1000.0f)
        blink = 0.0f;
    if (statusTimer > 0.0f)
        statusTimer -= dt;
}

bool MUi_Busy(void)
{
    return statusTimer > 0.0f || tf.buf != NULL || nf.down || barDrag != NULL || modal.kind != MODAL_NONE;
}

bool MUi_KeysFree(void)
{
    return !keysTaken && !tf.buf && modal.kind == MODAL_NONE;
}

bool MUi_MouseFree(void)
{
    return !mouseTaken && modal.kind == MODAL_NONE && !nf.down;
}

void MUi_TakeMouse(void)
{
    mouseTaken = true;
}

bool MUi_Ctrl(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
}

bool MUi_Shift(void)
{
    return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

bool MUi_Rep(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

Vector2 MUi_Mouse(void)
{
    return RayGame_MouseWorld();
}

bool MUi_Hover(Rectangle r)
{
    return CheckCollisionPointRec(MUi_Mouse(), r);
}

static bool Pressed(Rectangle r)
{
    return !Blocked() && !mouseTaken && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && MUi_Hover(r);
}

/* ------------------------------------------------------------------------- */
/* Texto e recorte                                                           */
/* ------------------------------------------------------------------------- */

void MUi_Text(const char *t, float x, float y, float size, Color c)
{
    DrawTextEx(mainFont, t, (Vector2){x, y}, size, 1.0f, c);
}

void MUi_TextShadow(const char *t, float x, float y, float size, Color c)
{
    DrawTextEx(mainFont, t, (Vector2){x + 2.0f, y + 2.0f}, size, 1.0f, (Color){0, 0, 0, 170});
    DrawTextEx(mainFont, t, (Vector2){x, y}, size, 1.0f, c);
}

float MUi_TextWidth(const char *t, float size)
{
    return MeasureTextEx(mainFont, t, size, 1.0f).x;
}

void MUi_TextCenter(const char *t, Rectangle r, float size, Color c)
{
    const float w = MUi_TextWidth(t, size);
    MUi_Text(t, floorf(r.x + (r.width - w) * 0.5f), floorf(r.y + (r.height - size) * 0.5f), size, c);
}

void MUi_TextRight(const char *t, float right, float y, float size, Color c)
{
    MUi_Text(t, right - MUi_TextWidth(t, size), y, size, c);
}

static void Scissor(Rectangle r)
{
    BeginScissorMode((int)r.x, (int)r.y, (int)ceilf(r.width), (int)ceilf(r.height));
}

void MUi_ClipBegin(Rectangle r)
{
    if (clipDepth > 0) {
        const Rectangle p = clipStack[clipDepth - 1];
        const float x0 = fmaxf(r.x, p.x), y0 = fmaxf(r.y, p.y);
        const float x1 = fminf(r.x + r.width, p.x + p.width), y1 = fminf(r.y + r.height, p.y + p.height);
        r = (Rectangle){x0, y0, fmaxf(x1 - x0, 0.0f), fmaxf(y1 - y0, 0.0f)};
    }
    if (clipDepth < CLIP_MAX)
        clipStack[clipDepth++] = r;
    Scissor(r);
}

void MUi_ClipEnd(void)
{
    if (clipDepth > 0)
        --clipDepth;
    if (clipDepth > 0)
        Scissor(clipStack[clipDepth - 1]);
    else
        EndScissorMode();
}

/* ------------------------------------------------------------------------- */
/* Molduras                                                                  */
/* ------------------------------------------------------------------------- */

static void Box(Rectangle r, Color fill, Color edge, float radius)
{
    const float side = fminf(r.width, r.height);
    const float round = side > radius * 2.0f ? radius * 2.0f / side : 1.0f;
    DrawRectangleRounded(r, round, 6, fill);
    if (edge.a)
        DrawRectangleRoundedLinesEx(r, round, 6, 1.0f, edge);
}

void MUi_DrawBackdrop(void)
{
    DrawRectangleGradientV(0, 0, (int)MUI_W, (int)MUI_H, MUI_C_BG_TOP, MUI_C_BG_BOT);
}

void MUi_DrawTopBar(const char *title, const char *detail, bool dirty)
{
    DrawRectangle(0, 0, (int)MUI_W, (int)MUI_TOP_H, MUI_C_BAR);
    DrawRectangle(0, (int)MUI_TOP_H - 2, (int)MUI_W, 2, (Color){255, 92, 176, 120});
    MUi_TextShadow(title, 16.0f, 11.0f, 26.0f, MUI_C_TEXT);

    float x = 16.0f + MUi_TextWidth(title, 26.0f) + 16.0f;
    if (detail && detail[0]) {
        MUi_Text(detail, x, 17.0f, 18.0f, MUI_C_DIM);
        x += MUi_TextWidth(detail, 18.0f) + 12.0f;
    }
    if (dirty)
        MUi_Text("* unsaved", x, 17.0f, 18.0f, MUI_C_HINT);
}

void MUi_DrawBottomBar(void)
{
    const float y = MUI_H - MUI_BOTTOM_H;
    DrawRectangle(0, (int)y, (int)MUI_W, (int)MUI_BOTTOM_H, MUI_C_BAR);
    if (statusTimer > 0.0f && statusText[0]) {
        static const Color tones[4] = {MUI_C_TEXT, MUI_C_OK, MUI_C_HINT, MUI_C_BAD};
        Color c = tones[statusTone];
        if (statusTimer < 0.5f)
            c.a = (unsigned char)(255.0f * statusTimer / 0.5f);
        DrawRectangle(0, (int)y, 4, (int)MUI_BOTTOM_H, c);
        MUi_Text(statusText, 14.0f, y + 6.0f, 18.0f, c);
    } else {
        MUi_TextRight("TAB shortcuts", MUI_W - 14.0f, y + 8.0f, 15.0f, MUI_C_FAINT);
    }
}

/* ------------------------------------------------------------------------- */
/* Overlay de atalhos: mesmo cartao do chart editor (tecla + o que faz, 2 colunas) */
/* ------------------------------------------------------------------------- */

static bool helpOpen;

bool MUi_HelpOpen(void)
{
    return helpOpen;
}

bool MUi_HelpUpdate(void)
{
    if (helpOpen) {
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE))
            helpOpen = false;
        keysTaken = true;
        return true;
    }
    if (MUi_KeysFree() && IsKeyPressed(KEY_TAB)) {
        helpOpen = true;
        keysTaken = true;
        return true;
    }
    return false;
}

void MUi_DrawHelp(const char *title, const MUiHelpRow *rows, int count)
{
    if (!helpOpen || count <= 0)
        return;
    const int perCol = (count + 1) / 2;
    DrawRectangle(0, 0, (int)MUI_W, (int)MUI_H, (Color){0, 0, 0, 215});
    const Rectangle card = {170.0f, 110.0f, 940.0f, 110.0f + perCol * 44.0f};
    Box(card, MUI_C_PANEL, MUI_C_EDGE, 12.0f);
    MUi_TextShadow(title, card.x + 30.0f, card.y + 22.0f, 24.0f, MUI_C_TEXT);
    MUi_TextRight("TAB or Esc closes", card.x + card.width - 24.0f, card.y + 30.0f, 18.0f, MUI_C_DIM);
    for (int i = 0; i < count; i++) {
        const float cx = card.x + 30.0f + (float)(i / perCol) * 460.0f;
        const float cy = card.y + 76.0f + (float)(i % perCol) * 44.0f;
        const Rectangle pill = {cx, cy, 150.0f, 32.0f};
        Box(pill, MUI_C_BTN, MUI_C_EDGE, 6.0f);
        MUi_Text(rows[i].key, pill.x + 10.0f, pill.y + 7.0f, 18.0f, MUI_C_TEXT);
        MUi_Text(rows[i].what, pill.x + 166.0f, pill.y + 6.0f, 20.0f, MUI_C_DIM);
    }
}

void MUi_DrawPanel(Rectangle r)
{
    Box(r, MUI_C_PANEL, MUI_C_EDGE, 8.0f);
}

void MUi_DrawSection(float x, float y, const char *title)
{
    DrawRectangleRounded((Rectangle){x, y + 2.0f, 4.0f, 16.0f}, 0.5f, 4, MUI_C_ACCENT);
    MUi_Text(title, x + 10.0f, y, 19.0f, MUI_C_TEXT);
}

void MUi_SetStatus(MUiTone tone, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(statusText, sizeof(statusText), fmt, args);
    va_end(args);
    statusTone = tone;
    statusTimer = STATUS_TIME;
}

/* ------------------------------------------------------------------------- */
/* Botoes                                                                    */
/* ------------------------------------------------------------------------- */

bool MUi_Button(Rectangle r)
{
    if (!Pressed(r))
        return false;
    mouseTaken = true;
    return true;
}

void MUi_DrawButton(Rectangle r, const char *label, MUiBtnStyle style)
{
    const bool hover = style != MUI_BTN_OFF && HoverVisible(r);
    Color fill = hover ? MUI_C_BTN_HOV : MUI_C_BTN;
    Color edge = MUI_C_EDGE;
    Color text = MUI_C_TEXT;

    switch (style) {
        case MUI_BTN_ACCENT:
            fill = hover ? (Color){255, 120, 190, 255} : MUI_C_ACCENT;
            edge = (Color){0, 0, 0, 0};
            break;
        case MUI_BTN_ACTIVE:
            fill = hover ? (Color){120, 60, 120, 255} : (Color){96, 44, 100, 255};
            edge = MUI_C_ACCENT;
            break;
        case MUI_BTN_DANGER:
            fill = hover ? (Color){150, 40, 64, 255} : (Color){70, 24, 40, 245};
            edge = (Color){255, 100, 110, 120};
            break;
        case MUI_BTN_OFF:
            text = MUI_C_FAINT;
            edge = (Color){120, 100, 200, 60};
            break;
        default:
            break;
    }
    Box(r, fill, edge, 6.0f);
    const float size = fminf(18.0f, r.height - 8.0f);
    MUi_TextCenter(label, r, size, text);
}

bool MUi_Check(Rectangle r, bool *value)
{
    if (!MUi_Button(r))
        return false;
    *value = !*value;
    return true;
}

void MUi_DrawCheck(Rectangle r, const char *label, bool value)
{
    const Rectangle box = {r.x, r.y + (r.height - 18.0f) * 0.5f, 18.0f, 18.0f};
    const bool hover = HoverVisible(r);
    Box(box, value ? MUI_C_ACCENT : MUI_C_FIELD, hover ? MUI_C_ACCENT : MUI_C_EDGE, 4.0f);
    if (value) {
        DrawLineEx((Vector2){box.x + 4.0f, box.y + 9.0f}, (Vector2){box.x + 8.0f, box.y + 13.0f}, 2.0f, WHITE);
        DrawLineEx((Vector2){box.x + 8.0f, box.y + 13.0f}, (Vector2){box.x + 14.0f, box.y + 5.0f}, 2.0f, WHITE);
    }
    if (label)
        MUi_Text(label, box.x + 26.0f, r.y + (r.height - 18.0f) * 0.5f, 18.0f, hover ? WHITE : MUI_C_TEXT);
}

static Rectangle TabRect(Rectangle r, int count, int i)
{
    const float w = r.width / (float)count;
    return (Rectangle){r.x + w * (float)i, r.y, w - 4.0f, r.height};
}

int MUi_Tabs(Rectangle r, int count, int current)
{
    for (int i = 0; i < count; ++i) {
        if (MUi_Button(TabRect(r, count, i)))
            return i;
    }
    return current;
}

void MUi_DrawTabs(Rectangle r, const char *const *names, int count, int current)
{
    for (int i = 0; i < count; ++i)
        MUi_DrawButton(TabRect(r, count, i), names[i], i == current ? MUI_BTN_ACTIVE : MUI_BTN_NORMAL);
}

void MUi_DrawSwatch(Rectangle r, Color c, bool selected)
{
    Box(r, c, selected ? MUI_C_ACCENT : (Color){255, 255, 255, 70}, 5.0f);
    if (selected)
        DrawRectangleRoundedLinesEx((Rectangle){r.x - 3.0f, r.y - 3.0f, r.width + 6.0f, r.height + 6.0f},
                                    0.3f, 6, 2.0f, MUI_C_ACCENT);
}

/* ------------------------------------------------------------------------- */
/* Campo de texto                                                            */
/* ------------------------------------------------------------------------- */

static int TfLen(void)
{
    return (int)strlen(tf.buf);
}

static int TfMin(void)
{
    return tf.caret < tf.anchor ? tf.caret : tf.anchor;
}

static int TfMax(void)
{
    return tf.caret > tf.anchor ? tf.caret : tf.anchor;
}

static bool IsCont(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

static int PrevCp(int i)
{
    if (i <= 0)
        return 0;
    --i;
    while (i > 0 && IsCont(tf.buf[i]))
        --i;
    return i;
}

static int NextCp(int i)
{
    const int len = TfLen();
    if (i >= len)
        return len;
    ++i;
    while (i < len && IsCont(tf.buf[i]))
        ++i;
    return i;
}

static bool IsWordChar(char c)
{
    const unsigned char u = (unsigned char)c;
    return u >= 0x80 || (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
}

static int PrevWord(int i)
{
    while (i > 0 && !IsWordChar(tf.buf[i - 1]))
        --i;
    while (i > 0 && IsWordChar(tf.buf[i - 1]))
        --i;
    return i;
}

static int NextWord(int i)
{
    const int len = TfLen();
    while (i < len && !IsWordChar(tf.buf[i]))
        ++i;
    while (i < len && IsWordChar(tf.buf[i]))
        ++i;
    return i;
}

static bool DeleteRange(int a, int b)
{
    if (a >= b)
        return false;
    memmove(tf.buf + a, tf.buf + b, strlen(tf.buf + b) + 1);
    tf.caret = tf.anchor = a;
    return true;
}

static bool DeleteSel(void)
{
    return DeleteRange(TfMin(), TfMax());
}

static bool AllowedByte(unsigned char c)
{
    if (c < 32 || c == 127)
        return false;
    if (tf.flags & MUI_TXT_NUMERIC)
        return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == ',';
    return true;
}

/* Insere n bytes no cursor (troca a selecao). Corta no limite sem partir um caractere UTF-8. */
static bool Insert(const char *s, size_t n)
{
    char clean[TF_BACKUP];
    size_t k = 0;
    for (size_t i = 0; i < n && k + 1 < sizeof(clean); ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '\n' || c == '\r')
            break;
        if (AllowedByte(c))
            clean[k++] = (c == ',' && (tf.flags & MUI_TXT_NUMERIC)) ? '.' : (char)c;
    }
    if (k == 0)
        return DeleteSel();

    DeleteSel();
    const size_t len = strlen(tf.buf);
    size_t room = tf.cap > len + 1 ? tf.cap - len - 1 : 0;
    if (k > room) {
        k = room;
        while (k > 0 && IsCont(clean[k]))
            --k;
    }
    if (k == 0)
        return false;

    memmove(tf.buf + tf.caret + k, tf.buf + tf.caret, len - (size_t)tf.caret + 1);
    memcpy(tf.buf + tf.caret, clean, k);
    tf.caret += (int)k;
    tf.anchor = tf.caret;
    return true;
}

static bool InsertCodepoint(int cp)
{
    char bytes[4];
    size_t n;
    if (cp < 32 || cp == 127 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return false;
    if (cp <= 0x7F) {
        bytes[0] = (char)cp;
        n = 1;
    } else if (cp <= 0x7FF) {
        bytes[0] = (char)(0xC0 | (cp >> 6));
        bytes[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp <= 0xFFFF) {
        bytes[0] = (char)(0xE0 | (cp >> 12));
        bytes[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        bytes[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        bytes[0] = (char)(0xF0 | (cp >> 18));
        bytes[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        bytes[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        bytes[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    return Insert(bytes, n);
}

static void CopySel(bool all)
{
    char tmp[TF_BACKUP];
    const int a = all ? 0 : TfMin();
    const int b = all ? TfLen() : TfMax();
    snprintf(tmp, sizeof(tmp), "%.*s", b - a, tf.buf + a);
    SetClipboardText(tmp);
}

static float FieldFont(Rectangle r)
{
    return fminf(19.0f, r.height - 10.0f);
}

static float PrefixWidth(const char *s, int n, float size)
{
    char tmp[TF_BACKUP];
    if (n <= 0)
        return 0.0f;
    snprintf(tmp, sizeof(tmp), "%.*s", n, s);
    return MUi_TextWidth(tmp, size);
}

/* Byte do texto mais perto do x do mouse. */
static int CaretAt(Rectangle r, float mx)
{
    const float size = FieldFont(r);
    const float x0 = r.x + TF_PAD - tf.scroll;
    const int len = TfLen();
    float prev = 0.0f;
    for (int i = 0; i < len; i = NextCp(i)) {
        const int next = NextCp(i);
        const float w = PrefixWidth(tf.buf, next, size);
        if (mx < x0 + (prev + w) * 0.5f)
            return i;
        prev = w;
    }
    return len;
}

static void Focus(char *buf, size_t cap, unsigned flags)
{
    if (tf.buf && tf.buf != buf)
        tf.lost = tf.buf;
    tf.buf = buf;
    tf.cap = cap;
    tf.flags = flags;
    tf.scroll = 0.0f;
    tf.selecting = false;
    tf.seen = true;
    snprintf(tf.backup, sizeof(tf.backup), "%s", buf);
    tf.anchor = 0;
    tf.caret = TfLen();
    blink = 0.0f;
}

void MUi_TextFocus(char *buf, size_t cap)
{
    Focus(buf, cap, 0);
}

void MUi_TextBlur(void)
{
    tf.buf = NULL;
    tf.selecting = false;
}

bool MUi_TextActive(void)
{
    return tf.buf != NULL;
}

bool MUi_TextIsFocused(const char *buf)
{
    return buf && tf.buf == buf;
}

MUiTextEvent MUi_TextField(Rectangle r, char *buf, size_t cap)
{
    return MUi_TextFieldEx(r, buf, cap, 0);
}

MUiTextEvent MUi_TextFieldEx(Rectangle r, char *buf, size_t cap, unsigned flags)
{
    if (tf.lost == buf) {
        tf.lost = NULL;
        return MUI_TXT_BLUR;
    }

    const Vector2 m = MUi_Mouse();
    if (tf.buf != buf) {
        if (Pressed(r)) {
            Focus(buf, cap, flags);
            tf.caret = tf.anchor = CaretAt(r, m.x);
            tf.selecting = true;
            mouseTaken = true;
        }
        return MUI_TXT_IDLE;
    }

    tf.seen = true;
    tf.flags = flags;
    keysTaken = true;
    const bool ctrl = MUi_Ctrl();
    const bool shift = MUi_Shift();
    bool changed = false;

    /* mouse: clique fora solta o foco mesmo que outro widget use o clique */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (!MUi_Hover(r)) {
            MUi_TextBlur();
            return MUI_TXT_BLUR;
        }
        if (!mouseTaken) {
            const double now = GetTime();
            static double lastClick;
            if (now - lastClick < DBL_CLICK_S) {
                tf.anchor = 0;
                tf.caret = TfLen();
            } else {
                tf.caret = CaretAt(r, m.x);
                if (!shift)
                    tf.anchor = tf.caret;
                tf.selecting = true;
            }
            lastClick = now;
            mouseTaken = true;
        }
    }
    if (tf.selecting) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            tf.caret = CaretAt(r, m.x);
        else
            tf.selecting = false;
    }

    /* teclado */
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        MUi_TextBlur();
        return MUI_TXT_SUBMIT;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        snprintf(buf, cap, "%s", tf.backup);
        MUi_TextBlur();
        return MUI_TXT_CANCEL;
    }
    if (IsKeyPressed(KEY_TAB)) {
        MUi_TextBlur();
        return MUI_TXT_BLUR;
    }

    if (ctrl && IsKeyPressed(KEY_A)) {
        tf.anchor = 0;
        tf.caret = TfLen();
    }
    if (ctrl && IsKeyPressed(KEY_C))
        CopySel(TfMin() == TfMax());
    if (ctrl && IsKeyPressed(KEY_X)) {
        const bool all = TfMin() == TfMax();
        CopySel(all);
        if (all) {
            tf.anchor = 0;
            tf.caret = TfLen();
        }
        changed |= DeleteSel();
    }
    if (ctrl && MUi_Rep(KEY_V)) {
        const char *clip = GetClipboardText();
        if (clip)
            changed |= Insert(clip, strlen(clip));
    }

    if (MUi_Rep(KEY_BACKSPACE)) {
        if (TfMin() != TfMax())
            changed |= DeleteSel();
        else
            changed |= DeleteRange(ctrl ? PrevWord(tf.caret) : PrevCp(tf.caret), tf.caret);
    }
    if (MUi_Rep(KEY_DELETE)) {
        if (TfMin() != TfMax())
            changed |= DeleteSel();
        else
            changed |= DeleteRange(tf.caret, ctrl ? NextWord(tf.caret) : NextCp(tf.caret));
    }

    int move = -1;
    if (MUi_Rep(KEY_LEFT)) {
        if (!shift && TfMin() != TfMax())
            move = TfMin();
        else
            move = ctrl ? PrevWord(tf.caret) : PrevCp(tf.caret);
    }
    if (MUi_Rep(KEY_RIGHT)) {
        if (!shift && TfMin() != TfMax())
            move = TfMax();
        else
            move = ctrl ? NextWord(tf.caret) : NextCp(tf.caret);
    }
    if (IsKeyPressed(KEY_HOME))
        move = 0;
    if (IsKeyPressed(KEY_END))
        move = TfLen();
    if (move >= 0) {
        tf.caret = move;
        if (!shift)
            tf.anchor = move;
        blink = 0.0f;
    }

    int cp;
    while ((cp = GetCharPressed()) > 0) {
        if (!ctrl)
            changed |= InsertCodepoint(cp);
    }
    if (changed)
        blink = 0.0f;
    return changed ? MUI_TXT_EDIT : MUI_TXT_IDLE;
}

void MUi_DrawTextField(Rectangle r, const char *buf, const char *hint)
{
    const bool focused = tf.buf == buf;
    const bool hover = HoverVisible(r);
    Box(r, MUI_C_FIELD, focused ? MUI_C_ACCENT : (hover ? (Color){170, 150, 240, 200} : MUI_C_EDGE), 5.0f);

    const float size = FieldFont(r);
    const float ty = floorf(r.y + (r.height - size) * 0.5f);
    MUi_ClipBegin((Rectangle){r.x + 3.0f, r.y + 2.0f, r.width - 6.0f, r.height - 4.0f});

    if (!focused) {
        if (!buf[0] && hint)
            MUi_Text(hint, r.x + TF_PAD, ty, size, MUI_C_FAINT);
        else
            MUi_Text(buf, r.x + TF_PAD, ty, size, MUI_C_TEXT);
        MUi_ClipEnd();
        return;
    }

    const float inner = r.width - TF_PAD * 2.0f;
    const float caretX = PrefixWidth(buf, tf.caret, size);
    const float fullW = MUi_TextWidth(buf, size);
    if (caretX - tf.scroll > inner)
        tf.scroll = caretX - inner;
    if (caretX - tf.scroll < 0.0f)
        tf.scroll = caretX;
    if (fullW - tf.scroll < inner)
        tf.scroll = fmaxf(0.0f, fullW - inner);

    const float x0 = r.x + TF_PAD - tf.scroll;
    if (TfMin() != TfMax()) {
        const float a = PrefixWidth(buf, TfMin(), size);
        const float b = PrefixWidth(buf, TfMax(), size);
        DrawRectangleRec((Rectangle){x0 + a, ty - 1.0f, b - a + 2.0f, size + 2.0f}, (Color){255, 92, 176, 110});
    }
    MUi_Text(buf, x0, ty, size, WHITE);
    if (fmodf(blink, 1.0f) < 0.6f)
        DrawRectangleRec((Rectangle){x0 + caretX, ty, 2.0f, size}, MUI_C_ACCENT);
    MUi_ClipEnd();
}

/* ------------------------------------------------------------------------- */
/* Numero                                                                    */
/* ------------------------------------------------------------------------- */

static int Decimals(float step)
{
    if (step >= 0.999f)
        return 0;
    if (step >= 0.099f)
        return 1;
    if (step >= 0.0099f)
        return 2;
    return 3;
}

static void FormatNum(char *out, size_t cap, float v, float step)
{
    snprintf(out, cap, "%.*f", Decimals(step), (double)v);
    if (strcmp(out, "-0") == 0)
        snprintf(out, cap, "0");
}

static float ClampF(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool MUi_NumBusy(void)
{
    return nf.down && nf.drag;
}

bool MUi_Num(Rectangle r, float *value, float step, float lo, float hi)
{
    const bool mine = nf.x == r.x && nf.y == r.y;

    if (nf.typing && mine) {
        nf.seen = true;
        const MUiTextEvent ev = MUi_TextFieldEx(r, nf.buf, sizeof(nf.buf), MUI_TXT_NUMERIC);
        if (ev == MUI_TXT_SUBMIT || ev == MUI_TXT_BLUR) {
            nf.typing = false;
            char *end = NULL;
            const float v = strtof(nf.buf, &end);
            if (end != nf.buf && isfinite(v)) {
                const float c = ClampF(v, lo, hi);
                if (c != *value) {
                    *value = c;
                    return true;
                }
            }
        } else if (ev == MUI_TXT_CANCEL || !MUi_TextIsFocused(nf.buf)) {
            nf.typing = false;
        }
        return false;
    }

    if (nf.down && mine) {
        nf.seen = true;
        mouseTaken = true;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const float dx = MUi_Mouse().x - nf.startMouse;
            if (!nf.drag && fabsf(dx) >= DRAG_PX)
                nf.drag = true;
            if (nf.drag) {
                const float mult = MUi_Shift() ? 10.0f : 1.0f;
                const float v = ClampF(nf.startValue + roundf(dx / DRAG_PX) * step * mult, lo, hi);
                if (v != *value) {
                    *value = v;
                    return true;
                }
            }
            return false;
        }
        nf.down = false;
        if (!nf.drag) {
            nf.typing = true;
            FormatNum(nf.buf, sizeof(nf.buf), *value, step);
            Focus(nf.buf, sizeof(nf.buf), MUI_TXT_NUMERIC);
        }
        nf.drag = false;
        return false;
    }

    if (Blocked() || nf.typing || nf.down)
        return false;

    if (Pressed(r)) {
        nf.x = r.x;
        nf.y = r.y;
        nf.down = true;
        nf.drag = false;
        nf.seen = true;
        nf.startMouse = MUi_Mouse().x;
        nf.startValue = *value;
        mouseTaken = true;
        return false;
    }

    if (MUi_Hover(r)) {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            const float mult = MUi_Shift() ? 10.0f : 1.0f;
            const float v = ClampF(*value + (wheel > 0.0f ? step : -step) * mult, lo, hi);
            if (v != *value) {
                *value = v;
                return true;
            }
        }
    }
    return false;
}

bool MUi_NumInt(Rectangle r, int *value, int step, int lo, int hi)
{
    float v = (float)*value;
    if (!MUi_Num(r, &v, (float)step, (float)lo, (float)hi))
        return false;
    const int n = (int)lroundf(v);
    if (n == *value)
        return false;
    *value = n;
    return true;
}

void MUi_DrawNum(Rectangle r, float value, float step)
{
    const bool mine = nf.x == r.x && nf.y == r.y;
    if (nf.typing && mine) {
        MUi_DrawTextField(r, nf.buf, NULL);
        return;
    }

    const bool active = nf.down && mine;
    const bool hover = active || HoverVisible(r);
    Box(r, MUI_C_FIELD, active ? MUI_C_ACCENT : (hover ? (Color){170, 150, 240, 200} : MUI_C_EDGE), 5.0f);

    /* setinhas de arrastar */
    const Color arrow = hover ? MUI_C_DIM : MUI_C_FAINT;
    const float cy = r.y + r.height * 0.5f;
    DrawTriangle((Vector2){r.x + 4.0f, cy}, (Vector2){r.x + 9.0f, cy + 4.0f}, (Vector2){r.x + 9.0f, cy - 4.0f}, arrow);
    DrawTriangle((Vector2){r.x + r.width - 4.0f, cy}, (Vector2){r.x + r.width - 9.0f, cy - 4.0f},
                 (Vector2){r.x + r.width - 9.0f, cy + 4.0f}, arrow);

    char text[32];
    FormatNum(text, sizeof(text), value, step);
    MUi_TextCenter(text, r, FieldFont(r), MUI_C_TEXT);
}

/* ------------------------------------------------------------------------- */
/* Lista                                                                     */
/* ------------------------------------------------------------------------- */

#define LIST_BAR_W 8.0f

int MUi_ListRows(Rectangle r, float rowH)
{
    const int n = (int)((r.height - 4.0f) / rowH);
    return n < 1 ? 1 : n;
}

static void ListClampTop(MUiList *l, int rows, int count)
{
    const int maxTop = count > rows ? count - rows : 0;
    if (l->top > maxTop)
        l->top = maxTop;
    if (l->top < 0)
        l->top = 0;
}

void MUi_ListShow(MUiList *l, Rectangle r, float rowH, int count)
{
    const int rows = MUi_ListRows(r, rowH);
    if (count <= 0) {
        l->sel = 0;
        l->top = 0;
        return;
    }
    if (l->sel >= count)
        l->sel = count - 1;
    if (l->sel < 0)
        l->sel = 0;
    if (l->sel < l->top)
        l->top = l->sel;
    if (l->sel >= l->top + rows)
        l->top = l->sel - rows + 1;
    ListClampTop(l, rows, count);
}

int MUi_List(MUiList *l, Rectangle r, float rowH, int count, bool *dbl)
{
    if (dbl)
        *dbl = false;
    const int rows = MUi_ListRows(r, rowH);
    ListClampTop(l, rows, count);
    if (Blocked())
        return -1;

    const Vector2 m = MUi_Mouse();
    if (MUi_Hover(r) && !nf.down) {
        l->wheel += GetMouseWheelMove();
        while (l->wheel >= 1.0f) { l->wheel -= 1.0f; l->top -= 3; }
        while (l->wheel <= -1.0f) { l->wheel += 1.0f; l->top += 3; }
        ListClampTop(l, rows, count);
    }

    /* barra de rolagem: clicar ou arrastar */
    if (count > rows) {
        const Rectangle bar = {r.x + r.width - LIST_BAR_W - 2.0f, r.y, LIST_BAR_W + 2.0f, r.height};
        if (Pressed(bar)) {
            barDrag = l;
            mouseTaken = true;
        }
        if (barDrag == l) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const float t = (m.y - r.y) / r.height;
                l->top = (int)(t * (float)count) - rows / 2;
                ListClampTop(l, rows, count);
                mouseTaken = true;
                return -1;
            }
            barDrag = NULL;
        }
    }

    if (!Pressed(r))
        return -1;
    mouseTaken = true;
    const int idx = l->top + (int)((m.y - r.y - 2.0f) / rowH);
    if (idx < 0 || idx >= count)
        return -1;

    const double now = GetTime();
    if (dbl)
        *dbl = idx == l->sel && now - l->lastClick < DBL_CLICK_S;
    l->lastClick = now;
    l->sel = idx;
    return idx;
}

bool MUi_ListKeys(MUiList *l, Rectangle r, float rowH, int count)
{
    if (count <= 0)
        return false;
    const int rows = MUi_ListRows(r, rowH);
    const int old = l->sel;
    if (MUi_Rep(KEY_DOWN)) l->sel = l->sel + 1 >= count ? 0 : l->sel + 1;
    if (MUi_Rep(KEY_UP)) l->sel = l->sel <= 0 ? count - 1 : l->sel - 1;
    if (MUi_Rep(KEY_PAGE_DOWN)) l->sel += rows;
    if (MUi_Rep(KEY_PAGE_UP)) l->sel -= rows;
    if (IsKeyPressed(KEY_HOME)) l->sel = 0;
    if (IsKeyPressed(KEY_END)) l->sel = count - 1;
    if (l->sel == old)
        return false;
    MUi_ListShow(l, r, rowH, count);
    return true;
}

void MUi_DrawListBegin(Rectangle r)
{
    Box(r, MUI_C_FIELD, MUI_C_EDGE, 6.0f);
    MUi_ClipBegin((Rectangle){r.x + 1.0f, r.y + 1.0f, r.width - 2.0f, r.height - 2.0f});
}

Rectangle MUi_ListRow(const MUiList *l, Rectangle r, float rowH, int index)
{
    return (Rectangle){r.x + 3.0f, r.y + 2.0f + (float)(index - l->top) * rowH,
                       r.width - 6.0f - LIST_BAR_W, rowH - 2.0f};
}

void MUi_DrawListRow(Rectangle row, bool selected)
{
    if (selected) {
        DrawRectangleRounded(row, 0.25f, 4, MUI_C_SEL);
        DrawRectangleRec((Rectangle){row.x, row.y + 3.0f, 3.0f, row.height - 6.0f}, MUI_C_ACCENT);
    } else if (HoverVisible(row)) {
        DrawRectangleRounded(row, 0.25f, 4, MUI_C_HOVER);
    }
}

void MUi_DrawListEnd(const MUiList *l, Rectangle r, float rowH, int count)
{
    MUi_ClipEnd();
    const int rows = MUi_ListRows(r, rowH);
    if (count <= rows)
        return;
    const Rectangle track = {r.x + r.width - LIST_BAR_W - 2.0f, r.y + 4.0f, LIST_BAR_W - 2.0f, r.height - 8.0f};
    DrawRectangleRounded(track, 1.0f, 4, (Color){255, 255, 255, 14});
    const float h = fmaxf(track.height * (float)rows / (float)count, 18.0f);
    const float t = (float)l->top / (float)(count - rows);
    const Rectangle thumb = {track.x, track.y + (track.height - h) * t, track.width, h};
    DrawRectangleRounded(thumb, 1.0f, 4, barDrag == l ? MUI_C_ACCENT : (Color){170, 150, 240, 150});
}

/* ------------------------------------------------------------------------- */
/* Tela de escolha                                                           */
/* ------------------------------------------------------------------------- */

#define PICK_ROW_H 34.0f

static Rectangle PickSearch(Rectangle r)
{
    return (Rectangle){r.x, r.y, r.width, 40.0f};
}

static Rectangle PickList(Rectangle r)
{
    return (Rectangle){r.x, r.y + 50.0f, r.width, r.height - 50.0f};
}

static bool PickMatch(MUiLabelFn label, int i, const char *search)
{
    const char *detail = label(i, 1);
    return ModEd_ContainsCI(label(i, 0), search) || (detail && ModEd_ContainsCI(detail, search));
}

void MUi_PickerFilter(MUiPicker *p, int count, MUiLabelFn label)
{
    p->visibleCount = 0;
    for (int i = 0; i < count && p->visibleCount < MUI_PICK_MAX; ++i) {
        if (PickMatch(label, i, p->search))
            p->visible[p->visibleCount++] = i;
    }
    if (p->list.sel >= p->visibleCount)
        p->list.sel = 0;
    p->list.top = 0;
}

int MUi_PickerUpdate(MUiPicker *p, Rectangle r, int count, MUiLabelFn label)
{
    if (modal.kind != MODAL_NONE)
        return MUI_PICK_NONE;
    if (!MUi_TextIsFocused(p->search)) {
        Focus(p->search, sizeof(p->search), 0);
        tf.anchor = tf.caret;
    }

    const bool hadText = p->search[0] != '\0';
    const MUiTextEvent ev = MUi_TextField(PickSearch(r), p->search, sizeof(p->search));
    if (ev == MUI_TXT_EDIT) {
        p->list.sel = 0;
        MUi_PickerFilter(p, count, label);
    } else if (ev == MUI_TXT_CANCEL) {
        if (!hadText)
            return MUI_PICK_BACK;
        p->search[0] = '\0';
        MUi_PickerFilter(p, count, label);
    } else if (ev == MUI_TXT_SUBMIT) {
        if (p->visibleCount > 0)
            return p->visible[p->list.sel];
        return p->search[0] ? MUI_PICK_NEW : MUI_PICK_NONE;
    }

    const Rectangle list = PickList(r);
    if (MUi_Rep(KEY_DOWN) || MUi_Rep(KEY_UP) || MUi_Rep(KEY_PAGE_DOWN) || MUi_Rep(KEY_PAGE_UP))
        MUi_ListKeys(&p->list, list, PICK_ROW_H, p->visibleCount);
    const int clicked = MUi_List(&p->list, list, PICK_ROW_H, p->visibleCount, NULL);
    return clicked >= 0 ? p->visible[clicked] : MUI_PICK_NONE;
}

void MUi_DrawPicker(const MUiPicker *p, Rectangle r, MUiLabelFn label, const char *hint, const char *empty)
{
    MUi_DrawTextField(PickSearch(r), p->search, hint);

    const Rectangle list = PickList(r);
    const int rows = MUi_ListRows(list, PICK_ROW_H);
    MUi_DrawListBegin(list);
    for (int i = p->list.top; i < p->visibleCount && i <= p->list.top + rows; ++i) {
        const int index = p->visible[i];
        const Rectangle row = MUi_ListRow(&p->list, list, PICK_ROW_H, i);
        MUi_DrawListRow(row, i == p->list.sel);

        const char *name = label(index, 0);
        const char *detail = label(index, 1);
        const char *tag = label(index, 2);
        MUi_Text(name, row.x + 12.0f, row.y + 6.0f, 20.0f, MUI_C_TEXT);
        if (detail && detail[0])
            MUi_Text(detail, row.x + 24.0f + MUi_TextWidth(name, 20.0f), row.y + 9.0f, 16.0f, MUI_C_DIM);
        if (tag && tag[0])
            MUi_TextRight(tag, row.x + row.width - 10.0f, row.y + 9.0f, 15.0f, MUI_C_FAINT);
    }
    MUi_DrawListEnd(&p->list, list, PICK_ROW_H, p->visibleCount);

    if (p->visibleCount == 0 && empty)
        MUi_Text(empty, list.x + 14.0f, list.y + 12.0f, 18.0f, MUI_C_HINT);
}

/* ------------------------------------------------------------------------- */
/* Janelas                                                                   */
/* ------------------------------------------------------------------------- */

#define PROMPT_W 780.0f
#define PROMPT_H 214.0f
#define CONFIRM_W 600.0f
#define CONFIRM_H 180.0f

static Rectangle ModalRect(void)
{
    const float w = modal.kind == MODAL_PROMPT ? PROMPT_W : CONFIRM_W;
    const float h = modal.kind == MODAL_PROMPT ? PROMPT_H : CONFIRM_H;
    return (Rectangle){(MUI_W - w) * 0.5f, (MUI_H - h) * 0.5f - 20.0f, w, h};
}

static Rectangle PromptField(void)
{
    const Rectangle w = ModalRect();
    return (Rectangle){w.x + 20.0f, w.y + 84.0f, w.width - 40.0f, 38.0f};
}

/* Botoes da direita para a esquerda: 0 = principal. */
static Rectangle ModalButton(int i)
{
    const Rectangle w = ModalRect();
    const float bw = 140.0f;
    return (Rectangle){w.x + w.width - 20.0f - bw - (bw + 10.0f) * (float)i, w.y + w.height - 52.0f, bw, 36.0f};
}

void MUi_PromptOpen(int id, const char *title, const char *hint, const char *initial)
{
    modal.kind = MODAL_PROMPT;
    modal.id = id;
    snprintf(modal.title, sizeof(modal.title), "%s", title ? title : "");
    snprintf(modal.hint, sizeof(modal.hint), "%s", hint ? hint : "");
    snprintf(modal.text, sizeof(modal.text), "%s", initial ? initial : "");
    modal.error[0] = '\0';
    Focus(modal.text, sizeof(modal.text), 0);
}

void MUi_PromptReopen(const char *error)
{
    modal.kind = MODAL_PROMPT;
    snprintf(modal.error, sizeof(modal.error), "%s", error ? error : "");
    Focus(modal.text, sizeof(modal.text), 0);
    tf.anchor = tf.caret;
}

void MUi_ConfirmOpen(int id, const char *title, const char *message)
{
    MUi_TextBlur();
    modal.kind = MODAL_CONFIRM;
    modal.id = id;
    snprintf(modal.title, sizeof(modal.title), "%s", title ? title : "");
    snprintf(modal.hint, sizeof(modal.hint), "%s", message ? message : "");
    modal.error[0] = '\0';
}

bool MUi_ModalActive(void)
{
    return modal.kind != MODAL_NONE;
}

const char *MUi_PromptText(void)
{
    return modal.text;
}

MUiModalResult MUi_ModalUpdate(int *id)
{
    if (id)
        *id = modal.id;
    if (modal.kind == MODAL_NONE)
        return MUI_MODAL_NONE;

    MUiModalResult result = MUI_MODAL_NONE;
    inModal = true;

    if (modal.kind == MODAL_PROMPT) {
        if (!MUi_TextIsFocused(modal.text)) {
            Focus(modal.text, sizeof(modal.text), 0);
            tf.anchor = tf.caret;
        }
        const MUiTextEvent ev = MUi_TextField(PromptField(), modal.text, sizeof(modal.text));
        if (ev == MUI_TXT_SUBMIT)
            result = MUI_MODAL_OK;
        else if (ev == MUI_TXT_CANCEL)
            result = MUI_MODAL_CANCEL;
        else if (ev == MUI_TXT_EDIT)
            modal.error[0] = '\0';
        if (MUi_Button(ModalButton(0)))
            result = MUI_MODAL_OK;
        if (MUi_Button(ModalButton(1)))
            result = MUI_MODAL_CANCEL;
    } else {
        if (MUi_Button(ModalButton(0)) || (MUi_Ctrl() && IsKeyPressed(KEY_S)))
            result = MUI_MODAL_OK;
        if (MUi_Button(ModalButton(1)))
            result = MUI_MODAL_DISCARD;
        if (MUi_Button(ModalButton(2)) || IsKeyPressed(KEY_ESCAPE))
            result = MUI_MODAL_CANCEL;
    }

    inModal = false;
    mouseTaken = true;
    keysTaken = true;
    while (GetCharPressed() > 0) {}

    if (result != MUI_MODAL_NONE) {
        modal.kind = MODAL_NONE;
        MUi_TextBlur();
        /* tira espacos das pontas do texto confirmado */
        size_t len = strlen(modal.text);
        while (len > 0 && modal.text[len - 1] == ' ')
            modal.text[--len] = '\0';
        size_t start = 0;
        while (modal.text[start] == ' ')
            ++start;
        memmove(modal.text, modal.text + start, len - start + 1);
    }
    return result;
}

void MUi_DrawModal(void)
{
    if (modal.kind == MODAL_NONE)
        return;

    drawingModal = true;
    DrawRectangle(0, 0, (int)MUI_W, (int)MUI_H, (Color){0, 0, 0, 150});
    const Rectangle w = ModalRect();
    Box((Rectangle){w.x + 4.0f, w.y + 6.0f, w.width, w.height}, (Color){0, 0, 0, 120}, (Color){0, 0, 0, 0}, 10.0f);
    Box(w, (Color){18, 14, 38, 252}, MUI_C_ACCENT, 10.0f);
    MUi_TextShadow(modal.title, w.x + 20.0f, w.y + 16.0f, 24.0f, MUI_C_TEXT);

    if (modal.kind == MODAL_PROMPT) {
        MUi_Text(modal.hint, w.x + 20.0f, w.y + 54.0f, 16.0f, MUI_C_DIM);
        MUi_DrawTextField(PromptField(), modal.text, "type or paste (Ctrl+V)");
        if (modal.error[0])
            MUi_Text(modal.error, w.x + 20.0f, w.y + 130.0f, 17.0f, MUI_C_BAD);
        else
            MUi_Text("Ctrl+V paste   Ctrl+C copy   Ctrl+A select all   Enter confirms   Esc cancels",
                     w.x + 20.0f, w.y + 130.0f, 15.0f, MUI_C_FAINT);
        MUi_DrawButton(ModalButton(0), "OK", MUI_BTN_ACCENT);
        MUi_DrawButton(ModalButton(1), "Cancel", MUI_BTN_NORMAL);
    } else {
        MUi_Text(modal.hint, w.x + 20.0f, w.y + 62.0f, 18.0f, MUI_C_DIM);
        MUi_DrawButton(ModalButton(0), "Save", MUI_BTN_ACCENT);
        MUi_DrawButton(ModalButton(1), "Discard", MUI_BTN_DANGER);
        MUi_DrawButton(ModalButton(2), "Cancel", MUI_BTN_NORMAL);
    }
    drawingModal = false;
}
