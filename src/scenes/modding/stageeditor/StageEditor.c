/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Stage Editor: escolhe um stage (ou digita um nome novo) e posiciona as imagens do palco
 * sobre um canvas. Salva o .lua de sprites e o .json (defaultZoom) do stage.
 *
 * Controles (modo edicao):
 *   mouse esq. arrasta sprite | mouse dir. arrasta a camera | roda do mouse = escala do sprite
 *   Up/Down seleciona | I/J/K/L move (Shift = 10 px) | Z/X e C/V = scrollFactor X/Y
 *   E = frente/atras | Del remove | Enter = adiciona imagem | -/= zoom da camera
 *   [ / ] = defaultZoom do stage | Ctrl+S salva | ESC sai (2x se houver mudancas)
 */

#include "scenes/modding/stageeditor/StageEditor.h"
#include "scenes/modding/stageeditor/StageEditorIO.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "Render.h"
#include "Game.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout (canvas virtual 1280x720)                                          */
/* ------------------------------------------------------------------------- */

#define SE_FILTER_MAX 64
#define SE_PICK_ROWS  20
#define SE_PICK_X     60.0f
#define SE_PICK_Y     140.0f
#define SE_PICK_ROW_H 26.0f
#define SE_LIST_X     20.0f
#define SE_LIST_Y     130.0f
#define SE_LIST_W     240.0f
#define SE_LIST_ROW_H 22.0f
#define SE_LIST_ROWS  24

static const Rectangle kCanvas = {270.0f, 90.0f, 690.0f, 600.0f};
static const float kCanvasCX = 270.0f + 690.0f * 0.5f;
static const float kCanvasCY = 90.0f + 600.0f * 0.5f;

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

typedef enum {
    SE_PICK = 0,
    SE_EDIT,
    SE_ADD
} SeMode;

static RayGraphicObject bg;
static Camera2D uiCam;
static SeMode mode;

static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
static int rootCount;

/* escolha de stage */
static StageEdEntry entries[STAGEED_LIST_MAX];
static int entryCount;
static char filter[SE_FILTER_MAX];
static int pickVisible[STAGEED_LIST_MAX];
static int pickVisibleCount;
static int pickSel;
static int pickTop;

/* stage aberto */
static StageEdEntry current;
static bool hasCurrent;
static StageSprite sprites[STAGE_MAX_SPRITES];
static Texture2D tex[STAGE_MAX_SPRITES];
static int spriteCount;
static int sel;
static int listTop;
static StageMeta meta;
static bool dirty;
static bool escArmed;

/* picker de imagens */
static char imageNames[STAGEED_LIST_MAX][MODED_NAME_MAX];
static int imageCount;
static char addFilter[SE_FILTER_MAX];
static int addVisible[STAGEED_LIST_MAX];
static int addVisibleCount;
static int addSel;
static int addTop;

/* camera do canvas: ponto do mundo no centro do canvas + zoom */
static Vector2 camTarget;
static float camZoom = 0.5f;
static bool dragging;
static bool panning;
static Vector2 dragWorld0;
static Vector2 dragSprite0;
static Vector2 panLast;

static char status[192];
static float statusTimer;

/* ------------------------------------------------------------------------- */
/* Texto e status                                                            */
/* ------------------------------------------------------------------------- */

static void SE_Text(const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(mainFont, text, (Vector2){x, y}, size, 1.0f, color);
}

static void SetStatus(const char *text)
{
    snprintf(status, sizeof(status), "%s", text);
    statusTimer = 4.0f;
}

/* "assets/mods/MeuMod" -> "MeuMod"; "assets" -> "assets". */
static const char *RootLabel(const char *root)
{
    const char *slash = strrchr(root, '/');
    const char *back = strrchr(root, '\\');
    if (back && (!slash || back > slash))
        slash = back;
    return slash ? slash + 1 : root;
}

static bool ContainsCI(const char *hay, const char *needle)
{
    const size_t n = strlen(needle);
    if (n == 0)
        return true;

    for (const char *h = hay; *h; ++h) {
        size_t i = 0;
        while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == n)
            return true;
    }
    return false;
}

static bool KeyHeld(int a, int b)
{
    return IsKeyDown(a) || IsKeyDown(b);
}

static bool Pressed(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

/* ------------------------------------------------------------------------- */
/* Listas                                                                    */
/* ------------------------------------------------------------------------- */

static void Rebuild_Pick(void)
{
    pickVisibleCount = 0;
    for (int i = 0; i < entryCount; ++i) {
        if (ContainsCI(entries[i].name, filter))
            pickVisible[pickVisibleCount++] = i;
    }
    if (pickSel >= pickVisibleCount)
        pickSel = pickVisibleCount > 0 ? pickVisibleCount - 1 : 0;
    pickTop = 0;
}

static void Rebuild_Add(void)
{
    addVisibleCount = 0;
    for (int i = 0; i < imageCount; ++i) {
        if (ContainsCI(imageNames[i], addFilter))
            addVisible[addVisibleCount++] = i;
    }
    if (addSel >= addVisibleCount)
        addSel = addVisibleCount > 0 ? addVisibleCount - 1 : 0;
    addTop = 0;
}

static void ScrollTo(int *top, int selIndex, int rows)
{
    if (selIndex < *top)
        *top = selIndex;
    if (selIndex >= *top + rows)
        *top = selIndex - rows + 1;
    if (*top < 0)
        *top = 0;
}

static void MoveSel(int *index, int count, int delta)
{
    if (count <= 0)
        return;
    *index = (*index + delta + count) % count;
}

/* ------------------------------------------------------------------------- */
/* Sprites e texturas                                                        */
/* ------------------------------------------------------------------------- */

static void Textures_FreeAt(int i)
{
    if (tex[i].id > 0)
        UnloadTexture(tex[i]);
    tex[i] = (Texture2D){0};
}

static void Textures_FreeAll(void)
{
    for (int i = 0; i < STAGE_MAX_SPRITES; ++i)
        Textures_FreeAt(i);
}

static void Texture_LoadAt(int i)
{
    char path[MODED_ROOT_MAX + MODED_NAME_MAX + 32];

    Textures_FreeAt(i);
    if (sprites[i].isGraphic)
        return;
    if (StageEd_ImagePath(current.root, sprites[i].image, path, sizeof(path)))
        tex[i] = Render_LoadTexture(path);
}

/* Tamanho desenhado no mundo: graphic usa graphicWidth; imagem usa o tamanho da textura (ou 100). */
static void SpriteSize(int i, float *w, float *h)
{
    const StageSprite *s = &sprites[i];
    float baseW = 100.0f, baseH = 100.0f;

    if (s->isGraphic) {
        baseW = s->graphicWidth;
        baseH = s->graphicHeight;
    } else if (tex[i].id > 0) {
        baseW = (float)tex[i].width;
        baseH = (float)tex[i].height;
    }
    *w = baseW * s->scaleX;
    *h = baseH * s->scaleY;
}

static Rectangle SpriteRectWorld(int i)
{
    float w, h;
    SpriteSize(i, &w, &h);
    return (Rectangle){sprites[i].x, sprites[i].y, w, h};
}

static Vector2 WorldToScreen(Vector2 p)
{
    return (Vector2){kCanvasCX + (p.x - camTarget.x) * camZoom,
                     kCanvasCY + (p.y - camTarget.y) * camZoom};
}

static Vector2 ScreenToWorld(Vector2 p)
{
    return (Vector2){camTarget.x + (p.x - kCanvasCX) / camZoom,
                     camTarget.y + (p.y - kCanvasCY) / camZoom};
}

static int HitTest(Vector2 world)
{
    for (int i = spriteCount - 1; i >= 0; --i) {
        if (CheckCollisionPointRec(world, SpriteRectWorld(i)))
            return i;
    }
    return -1;
}

/* Enquadra os sprites no canvas. */
static void FitView(void)
{
    if (spriteCount <= 0) {
        camTarget = (Vector2){0.0f, 0.0f};
        camZoom = 0.5f;
        return;
    }

    Rectangle first = SpriteRectWorld(0);
    float minX = first.x, minY = first.y, maxX = first.x + first.width, maxY = first.y + first.height;
    for (int i = 1; i < spriteCount; ++i) {
        const Rectangle r = SpriteRectWorld(i);
        if (r.x < minX) minX = r.x;
        if (r.y < minY) minY = r.y;
        if (r.x + r.width > maxX) maxX = r.x + r.width;
        if (r.y + r.height > maxY) maxY = r.y + r.height;
    }

    const float bw = maxX - minX + 200.0f;
    const float bh = maxY - minY + 200.0f;
    float zoom = fminf(kCanvas.width / bw, kCanvas.height / bh);
    camZoom = fminf(fmaxf(zoom, 0.1f), 2.0f);
    camTarget = (Vector2){(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
}

static void MarkDirty(void)
{
    dirty = true;
    escArmed = false;
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar / remover                                                  */
/* ------------------------------------------------------------------------- */

static void OpenEntry(const StageEdEntry *entry)
{
    char msg[160] = {0};

    current = *entry;
    hasCurrent = true;
    if (!StageEd_Load(&current, sprites, STAGE_MAX_SPRITES, &spriteCount, &meta, msg, sizeof(msg))) {
        SetStatus("nao consegui ler o stage");
        hasCurrent = false;
        return;
    }

    Textures_FreeAll();
    for (int i = 0; i < spriteCount; ++i)
        Texture_LoadAt(i);

    FitView();
    sel = spriteCount > 0 ? 0 : -1;
    listTop = 0;
    dirty = false;
    escArmed = false;
    mode = SE_EDIT;
    SetStatus(msg);
}

static void SaveCurrent(void)
{
    char msg[256] = {0};
    const bool ok = StageEd_Save(&current, sprites, spriteCount, &meta, msg, sizeof(msg));
    dirty = !ok;
    if (ok)
        escArmed = false;
    SetStatus(msg);
}

static void RemoveSelected(void)
{
    if (sel < 0 || sel >= spriteCount)
        return;

    Textures_FreeAt(sel);
    for (int i = sel; i < spriteCount - 1; ++i) {
        sprites[i] = sprites[i + 1];
        tex[i] = tex[i + 1];
    }
    tex[spriteCount - 1] = (Texture2D){0};
    sprites[spriteCount - 1] = (StageSprite){0};
    --spriteCount;

    if (sel >= spriteCount)
        sel = spriteCount - 1;
    MarkDirty();
}

static void AddImage(const char *image)
{
    if (spriteCount >= STAGE_MAX_SPRITES) {
        SetStatus("limite de sprites atingido");
        return;
    }

    const int i = spriteCount;
    StageSprite *s = &sprites[i];
    memset(s, 0, sizeof(*s));
    snprintf(s->image, sizeof(s->image), "%s", image);
    s->scaleX = 1.0f;
    s->scaleY = 1.0f;
    s->scrollX = 1.0f;
    s->scrollY = 1.0f;
    s->front = 0;
    ++spriteCount;

    Texture_LoadAt(i);
    float w, h;
    SpriteSize(i, &w, &h);
    s->x = camTarget.x - w * 0.5f;
    s->y = camTarget.y - h * 0.5f;

    sel = i;
    MarkDirty();
}

/* ------------------------------------------------------------------------- */
/* Update: escolha de stage                                                  */
/* ------------------------------------------------------------------------- */

static void CreateNewFromFilter(void)
{
    if (!ModEd_IsSafeName(filter)) {
        SetStatus("nome invalido: use letras, digitos, - _ . (sem espacos)");
        return;
    }

    StageEdEntry entry = {0};
    snprintf(entry.name, sizeof(entry.name), "%s", filter);
    snprintf(entry.root, sizeof(entry.root), "%s", roots[0]);
    OpenEntry(&entry);
}

static void Pick_Update(void)
{
    if (ModEd_TypeText(filter, sizeof(filter)))
        Rebuild_Pick();

    if (IsKeyPressed(KEY_ESCAPE)) {
        ModdingMenu_SetScene();
        return;
    }

    if (Pressed(KEY_DOWN))
        MoveSel(&pickSel, pickVisibleCount, 1);
    if (Pressed(KEY_UP))
        MoveSel(&pickSel, pickVisibleCount, -1);
    ScrollTo(&pickTop, pickSel, SE_PICK_ROWS);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (pickVisibleCount > 0)
            OpenEntry(&entries[pickVisible[pickSel]]);
        else
            CreateNewFromFilter();
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 m = RayGame_MouseWorld();
        if (m.x >= SE_PICK_X && m.x < SE_PICK_X + 700.0f && m.y >= SE_PICK_Y) {
            const int row = (int)((m.y - SE_PICK_Y) / SE_PICK_ROW_H) + pickTop;
            if (row >= 0 && row < pickVisibleCount)
                OpenEntry(&entries[pickVisible[row]]);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Update: picker de imagens (ADD)                                           */
/* ------------------------------------------------------------------------- */

static void Add_Update(void)
{
    if (ModEd_TypeText(addFilter, sizeof(addFilter)))
        Rebuild_Add();

    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = SE_EDIT;
        return;
    }
    if (Pressed(KEY_DOWN))
        MoveSel(&addSel, addVisibleCount, 1);
    if (Pressed(KEY_UP))
        MoveSel(&addSel, addVisibleCount, -1);
    ScrollTo(&addTop, addSel, SE_LIST_ROWS);

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && addVisibleCount > 0) {
        AddImage(imageNames[addVisible[addSel]]);
        mode = SE_EDIT;
    }
}

/* ------------------------------------------------------------------------- */
/* Update: edicao                                                            */
/* ------------------------------------------------------------------------- */

static void NudgeSelected(float dx, float dy)
{
    if (sel < 0)
        return;
    sprites[sel].x += dx;
    sprites[sel].y += dy;
    MarkDirty();
}

static void ScrollSelected(int axisX, float delta)
{
    if (sel < 0)
        return;
    StageSprite *s = &sprites[sel];
    if (axisX)
        s->scrollX = fminf(fmaxf(s->scrollX + delta, -10.0f), 10.0f);
    else
        s->scrollY = fminf(fmaxf(s->scrollY + delta, -10.0f), 10.0f);
    MarkDirty();
}

static void ScaleSelected(float factor)
{
    if (sel < 0)
        return;
    StageSprite *s = &sprites[sel];
    s->scaleX = fminf(fmaxf(s->scaleX * factor, 0.02f), 20.0f);
    s->scaleY = fminf(fmaxf(s->scaleY * factor, 0.02f), 20.0f);
    MarkDirty();
}

/* ------------------------------------------------------------------------- */
/* Painel de propriedades com mouse (campos arrastaveis + botoes)           */
/* ------------------------------------------------------------------------- */

#define SE_VAL_X 1100.0f
#define SE_VAL_W 150.0f

typedef enum {
    SF_X = 0,
    SF_Y,
    SF_SCALE,
    SF_SCROLLX,
    SF_SCROLLY,
    SF_META_ZOOM,
    SF_COUNT
} SeField;

static const char *const kFieldLabel[SF_COUNT] = {"x", "y", "escala", "scroll X", "scroll Y", "zoom padrao"};
static int activeField = -1;
static float fieldStartMouse;
static float fieldStartValue;

static const Rectangle kBtnFront = {985.0f, 360.0f, 138.0f, 28.0f};
static const Rectangle kBtnRemove = {1127.0f, 360.0f, 123.0f, 28.0f};
static const Rectangle kBtnAdd = {985.0f, 396.0f, 265.0f, 28.0f};
static const Rectangle kPadUp = {1110.0f, 440.0f, 50.0f, 30.0f};
static const Rectangle kPadLeft = {1050.0f, 478.0f, 50.0f, 30.0f};
static const Rectangle kPadRight = {1170.0f, 478.0f, 50.0f, 30.0f};
static const Rectangle kPadDown = {1110.0f, 516.0f, 50.0f, 30.0f};
static const Rectangle kZoomOut = {985.0f, 570.0f, 60.0f, 30.0f};
static const Rectangle kZoomIn = {1190.0f, 570.0f, 60.0f, 30.0f};
static const Rectangle kBtnSave = {985.0f, 620.0f, 265.0f, 34.0f};

static float Clampf(float v, float lo, float hi)
{
    return fminf(fmaxf(v, lo), hi);
}

static Rectangle FieldRect(int id)
{
    return (Rectangle){SE_VAL_X, 150.0f + id * 34.0f, SE_VAL_W, 26.0f};
}

static bool FieldEnabled(int id)
{
    return id == SF_META_ZOOM || sel >= 0;
}

static float FieldGet(int id)
{
    if (id == SF_META_ZOOM)
        return meta.defaultZoom;
    if (sel < 0)
        return 0.0f;

    const StageSprite *s = &sprites[sel];
    switch (id) {
        case SF_X: return s->x;
        case SF_Y: return s->y;
        case SF_SCALE: return s->scaleX;
        case SF_SCROLLX: return s->scrollX;
        default: return s->scrollY;
    }
}

static void FieldSet(int id, float v)
{
    if (id == SF_META_ZOOM) {
        meta.defaultZoom = Clampf(v, 0.05f, 10.0f);
        MarkDirty();
        return;
    }
    if (sel < 0)
        return;

    StageSprite *s = &sprites[sel];
    switch (id) {
        case SF_X: s->x = v; break;
        case SF_Y: s->y = v; break;
        case SF_SCALE: s->scaleX = s->scaleY = Clampf(v, 0.02f, 20.0f); break;
        case SF_SCROLLX: s->scrollX = Clampf(v, -10.0f, 10.0f); break;
        default: s->scrollY = Clampf(v, -10.0f, 10.0f); break;
    }
    MarkDirty();
}

/* Quanto o valor muda por pixel arrastado. */
static float FieldStep(int id)
{
    switch (id) {
        case SF_SCALE:
        case SF_META_ZOOM: return 0.01f;
        case SF_SCROLLX:
        case SF_SCROLLY: return 0.05f;
        default: return 1.0f;
    }
}

static bool Hover(Rectangle r)
{
    return CheckCollisionPointRec(RayGame_MouseWorld(), r);
}

static bool Clicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && Hover(r);
}

static void OpenAddPicker(void)
{
    addFilter[0] = '\0';
    addSel = 0;
    imageCount = StageEd_ScanImages(imageNames, STAGEED_LIST_MAX);
    Rebuild_Add();
    mode = SE_ADD;
}

static void Panel_Update(bool shift)
{
    const Vector2 m = RayGame_MouseWorld();
    const float mult = shift ? 10.0f : 1.0f;

    if (activeField >= 0) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            FieldSet(activeField, fieldStartValue + (m.x - fieldStartMouse) * FieldStep(activeField) * mult);
        else
            activeField = -1;
        return;
    }

    for (int id = 0; id < SF_COUNT; ++id) {
        if (FieldEnabled(id) && Clicked(FieldRect(id))) {
            activeField = id;
            fieldStartMouse = m.x;
            fieldStartValue = FieldGet(id);
            return;
        }
    }

    if (Clicked(kBtnFront) && sel >= 0) {
        sprites[sel].front = !sprites[sel].front;
        MarkDirty();
    }
    if (Clicked(kBtnRemove))
        RemoveSelected();
    if (Clicked(kBtnAdd))
        OpenAddPicker();

    if (Clicked(kPadUp)) NudgeSelected(0.0f, -mult);
    if (Clicked(kPadDown)) NudgeSelected(0.0f, mult);
    if (Clicked(kPadLeft)) NudgeSelected(-mult, 0.0f);
    if (Clicked(kPadRight)) NudgeSelected(mult, 0.0f);

    if (Clicked(kZoomOut)) camZoom = fmaxf(camZoom / 1.1f, 0.1f);
    if (Clicked(kZoomIn)) camZoom = fminf(camZoom * 1.1f, 4.0f);

    if (Clicked(kBtnSave))
        SaveCurrent();
}

static void DrawBtn(Rectangle r, const char *label, bool active)
{
    const bool hover = Hover(r);
    DrawRectangleRec(r, active ? (Color){30, 70, 40, 230} : (hover ? (Color){60, 60, 60, 230} : (Color){18, 18, 18, 220}));
    DrawRectangleLinesEx(r, 1, active ? GREEN : (hover ? WHITE : GRAY));
    const Vector2 size = MeasureTextEx(mainFont, label, 18.0f, 1.0f);
    SE_Text(label, r.x + (r.width - size.x) * 0.5f, r.y + (r.height - size.y) * 0.5f, 18.0f, WHITE);
}

static void Panel_Draw(void)
{
    DrawRectangleRec((Rectangle){965.0f, 95.0f, 300.0f, 560.0f}, (Color){20, 20, 26, 220});

    char line[64];
    if (sel >= 0)
        snprintf(line, sizeof(line), "%s", sprites[sel].image);
    else
        snprintf(line, sizeof(line), "nenhum sprite");
    SE_Text(line, 985.0f, 104.0f, 22.0f, sel >= 0 ? YELLOW : GRAY);
    SE_Text("arraste o numero para mudar", 985.0f, 128.0f, 15.0f, GRAY);

    for (int id = 0; id < SF_COUNT; ++id) {
        const Rectangle r = FieldRect(id);
        const bool on = FieldEnabled(id);
        SE_Text(kFieldLabel[id], 985.0f, r.y + 4.0f, 18.0f, on ? WHITE : DARKGRAY);
        DrawRectangleRec(r, activeField == id ? (Color){30, 70, 40, 230} : (Color){18, 18, 18, 220});
        DrawRectangleLinesEx(r, 1, Hover(r) ? WHITE : GRAY);
        if (!on) {
            SE_Text("-", r.x + 8.0f, r.y + 4.0f, 18.0f, DARKGRAY);
            continue;
        }
        const float v = FieldGet(id);
        if (id == SF_X || id == SF_Y)
            snprintf(line, sizeof(line), "%.0f", v);
        else
            snprintf(line, sizeof(line), "%.2f", v);
        SE_Text(line, r.x + 8.0f, r.y + 4.0f, 18.0f, WHITE);
    }

    DrawBtn(kBtnFront, sel >= 0 && sprites[sel].front ? "frente" : "atras", false);
    DrawBtn(kBtnRemove, "remover", false);
    DrawBtn(kBtnAdd, "+ imagem (Enter)", false);

    SE_Text("mover (Shift = 10)", 985.0f, 448.0f, 15.0f, GRAY);
    DrawBtn(kPadUp, "^", false);
    DrawBtn(kPadLeft, "<", false);
    DrawBtn(kPadRight, ">", false);
    DrawBtn(kPadDown, "v", false);

    DrawBtn(kZoomOut, "-", false);
    snprintf(line, sizeof(line), "zoom %d%%", (int)(camZoom * 100.0f + 0.5f));
    SE_Text(line, 1060.0f, 578.0f, 18.0f, WHITE);
    DrawBtn(kZoomIn, "+", false);

    DrawBtn(kBtnSave, "salvar (Ctrl+S)", dirty);
}

static void Edit_Update(void)
{
    const Vector2 m = RayGame_MouseWorld();
    const bool inCanvas = CheckCollisionPointRec(m, kCanvas);
    const bool ctrl = KeyHeld(KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL);
    const bool shift = KeyHeld(KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT);
    const float step = shift ? 10.0f : 1.0f;

    Panel_Update(shift);

    if (ctrl && IsKeyPressed(KEY_S)) {
        SaveCurrent();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty && !escArmed) {
            escArmed = true;
            SetStatus("ha mudancas nao salvas: ESC de novo sai sem salvar (Ctrl+S salva)");
            return;
        }
        Textures_FreeAll();
        hasCurrent = false;
        dirty = false;
        escArmed = false;
        mode = SE_PICK;
        entryCount = StageEd_ScanList(entries, STAGEED_LIST_MAX);
        Rebuild_Pick();
        return;
    }

    /* Mouse: arrastar sprite, arrastar camera, roda = escala. */
    if (inCanvas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 w = ScreenToWorld(m);
        const int hit = HitTest(w);
        sel = hit;
        if (hit >= 0) {
            dragging = true;
            dragWorld0 = w;
            dragSprite0 = (Vector2){sprites[hit].x, sprites[hit].y};
        }
    }
    if (dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && sel >= 0) {
            const Vector2 w = ScreenToWorld(m);
            const float nx = dragSprite0.x + (w.x - dragWorld0.x);
            const float ny = dragSprite0.y + (w.y - dragWorld0.y);
            if (nx != sprites[sel].x || ny != sprites[sel].y) {
                sprites[sel].x = nx;
                sprites[sel].y = ny;
                MarkDirty();
            }
        } else {
            dragging = false;
        }
    }

    if (inCanvas && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        panning = true;
        panLast = m;
    }
    if (panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            camTarget.x -= (m.x - panLast.x) / camZoom;
            camTarget.y -= (m.y - panLast.y) / camZoom;
            panLast = m;
        } else {
            panning = false;
        }
    }

    const float wheel = GetMouseWheelMove();
    if (inCanvas && wheel != 0.0f)
        ScaleSelected(1.0f + wheel * 0.08f);

    /* Clique na lista de sprites. */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.x >= SE_LIST_X && m.x < SE_LIST_X + SE_LIST_W &&
        m.y >= SE_LIST_Y) {
        const int row = (int)((m.y - SE_LIST_Y) / SE_LIST_ROW_H) + listTop;
        if (row >= 0 && row < spriteCount)
            sel = row;
    }

    /* Teclado. */
    if (Pressed(KEY_DOWN))
        MoveSel(&sel, spriteCount, 1);
    if (Pressed(KEY_UP))
        MoveSel(&sel, spriteCount, -1);
    if (IsKeyPressed(KEY_TAB) && spriteCount > 0)
        sel = (sel + 1) % spriteCount;
    if (sel >= 0)
        ScrollTo(&listTop, sel, SE_LIST_ROWS);

    if (!ctrl) {
        if (Pressed(KEY_J)) NudgeSelected(-step, 0.0f);
        if (Pressed(KEY_L)) NudgeSelected(step, 0.0f);
        if (Pressed(KEY_I)) NudgeSelected(0.0f, -step);
        if (Pressed(KEY_K)) NudgeSelected(0.0f, step);

        if (Pressed(KEY_Z)) ScrollSelected(1, -0.1f);
        if (Pressed(KEY_X)) ScrollSelected(1, 0.1f);
        if (Pressed(KEY_C)) ScrollSelected(0, -0.1f);
        if (Pressed(KEY_V)) ScrollSelected(0, 0.1f);

        if (IsKeyPressed(KEY_E) && sel >= 0) {
            sprites[sel].front = !sprites[sel].front;
            MarkDirty();
        }
    }

    if (IsKeyPressed(KEY_DELETE))
        RemoveSelected();

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        addFilter[0] = '\0';
        addSel = 0;
        imageCount = StageEd_ScanImages(imageNames, STAGEED_LIST_MAX);
        Rebuild_Add();
        mode = SE_ADD;
        return;
    }

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
        camZoom = fminf(camZoom * 1.1f, 4.0f);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        camZoom = fmaxf(camZoom / 1.1f, 0.1f);

    if (IsKeyPressed(KEY_LEFT_BRACKET)) {
        meta.defaultZoom = fmaxf(meta.defaultZoom - 0.05f, 0.05f);
        MarkDirty();
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
        meta.defaultZoom = fminf(meta.defaultZoom + 0.05f, 10.0f);
        MarkDirty();
    }
}

static void StageEditor_Update([[maybe_unused]] RayScene *scene)
{
    if (statusTimer > 0.0f)
        statusTimer -= RayGame_DeltaTime();

    if (mode == SE_PICK)
        Pick_Update();
    else if (mode == SE_ADD)
        Add_Update();
    else
        Edit_Update();
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

static void BeginUi(void)
{
    Render_SetCamera(&uiCam);
}

static void EndUi(void)
{
    Render_StopCamera();
}

/* Chamado dentro de BeginUi(): tudo em coordenadas virtuais 1280x720. */
static void DrawBackdrop(void)
{
    ClearBackground(BLACK);
    Render_DrawGraphicObject(&bg);
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 150});
}

static void DrawStatus(void)
{
    if (statusTimer > 0.0f && status[0])
        SE_Text(status, 20.0f, 50.0f, 20.0f, YELLOW);
}

static void Pick_Draw(void)
{
    SE_Text("STAGE EDITOR - escolha um stage", 20.0f, 20.0f, 28.0f, WHITE);
    SE_Text("filtro:", SE_PICK_X, 100.0f, 22.0f, GRAY);
    SE_Text(filter, SE_PICK_X + 80.0f, 100.0f, 22.0f, WHITE);
    DrawRectangle((int)(SE_PICK_X + 80.0f + MeasureTextEx(mainFont, filter, 22.0f, 1.0f).x + 2),
                  100, 2, 22, WHITE);

    for (int row = 0; row < SE_PICK_ROWS; ++row) {
        const int idx = pickTop + row;
        if (idx >= pickVisibleCount)
            break;
        const float y = SE_PICK_Y + row * SE_PICK_ROW_H;
        const bool on = idx == pickSel;
        if (on)
            DrawRectangle((int)SE_PICK_X - 6, (int)y - 2, 700, (int)SE_PICK_ROW_H, (Color){42, 209, 86, 60});
        const StageEdEntry *e = &entries[pickVisible[idx]];
        char line[160];
        snprintf(line, sizeof(line), "%s   [%s]", e->name, RootLabel(e->root));
        SE_Text(line, SE_PICK_X, y, 22.0f, on ? GREEN : WHITE);
    }

    if (entryCount == 0)
        SE_Text("nenhum stage encontrado: digite um nome e Enter para criar", SE_PICK_X, SE_PICK_Y, 22.0f, GRAY);
    else if (pickVisibleCount == 0 && filter[0])
        SE_Text("Enter cria o stage novo com este nome", SE_PICK_X, SE_PICK_Y, 22.0f, GRAY);

    SE_Text("Up/Down escolhe   Enter abre   ESC volta ao menu", 20.0f, 690.0f, 18.0f, GRAY);
    DrawStatus();
}

/* ------------------------------------------------------------------------- */
/* Draw: canvas e painel                                                     */
/* ------------------------------------------------------------------------- */

static void DrawCoverOutside(void)
{
    /* Sprites podem passar do canvas: pinta por cima das bordas (sem scissor). */
    const Color cover = {10, 10, 14, 255};
    const float right = kCanvas.x + kCanvas.width;
    const float bottom = kCanvas.y + kCanvas.height;
    DrawRectangle(0, 0, 1280, (int)kCanvas.y, cover);
    DrawRectangle(0, (int)bottom, 1280, 720 - (int)bottom, cover);
    DrawRectangle(0, (int)kCanvas.y, (int)kCanvas.x, (int)kCanvas.height, cover);
    DrawRectangle((int)right, (int)kCanvas.y, 1280 - (int)right, (int)kCanvas.height, cover);
}

static void Edit_DrawCanvas(void)
{
    DrawRectangleRec(kCanvas, (Color){16, 16, 22, 255});

    /* grade a cada 100 px do mundo */
    const float halfW = kCanvas.width * 0.5f / camZoom;
    const float halfH = kCanvas.height * 0.5f / camZoom;
    const int k0 = (int)ceilf((camTarget.x - halfW) / 100.0f);
    const int k1 = (int)floorf((camTarget.x + halfW) / 100.0f);
    for (int k = k0; k <= k1 && k - k0 < 400; ++k) {
        const float sx = WorldToScreen((Vector2){k * 100.0f, 0.0f}).x;
        DrawLine((int)sx, (int)kCanvas.y, (int)sx, (int)(kCanvas.y + kCanvas.height), (Color){40, 40, 52, 255});
    }
    const int j0 = (int)ceilf((camTarget.y - halfH) / 100.0f);
    const int j1 = (int)floorf((camTarget.y + halfH) / 100.0f);
    for (int j = j0; j <= j1 && j - j0 < 400; ++j) {
        const float sy = WorldToScreen((Vector2){0.0f, j * 100.0f}).y;
        DrawLine((int)kCanvas.x, (int)sy, (int)(kCanvas.x + kCanvas.width), (int)sy, (Color){40, 40, 52, 255});
    }

    for (int i = 0; i < spriteCount; ++i) {
        const Rectangle r = SpriteRectWorld(i);
        const Vector2 tl = WorldToScreen((Vector2){r.x, r.y});
        const Rectangle dst = {tl.x, tl.y, r.width * camZoom, r.height * camZoom};
        const StageSprite *s = &sprites[i];

        if (s->isGraphic) {
            DrawRectangleRec(dst, (Color){s->colorR, s->colorG, s->colorB, s->colorA});
        } else if (tex[i].id > 0) {
            /* Animado: desenha o atlas inteiro (so uma previa de posicao e escala). */
            DrawTexturePro(tex[i], (Rectangle){0, 0, (float)tex[i].width, (float)tex[i].height},
                           dst, (Vector2){0, 0}, 0.0f, WHITE);
        } else {
            DrawRectangleLinesEx(dst, 1.0f, RED);
        }

        SE_Text(s->image, dst.x + 2.0f, dst.y - 16.0f, 14.0f, i == sel ? YELLOW : GRAY);
        if (i == sel)
            DrawRectangleLinesEx(dst, 2.0f, YELLOW);
    }

    DrawCoverOutside();
    DrawRectangleLinesEx(kCanvas, 1.0f, GRAY);
}

static void Edit_DrawList(void)
{
    SE_Text("sprites", SE_LIST_X, SE_LIST_Y - 26.0f, 20.0f, GRAY);
    for (int row = 0; row < SE_LIST_ROWS; ++row) {
        const int idx = listTop + row;
        if (idx >= spriteCount)
            break;
        const float y = SE_LIST_Y + row * SE_LIST_ROW_H;
        const bool on = idx == sel;
        if (on)
            DrawRectangle((int)SE_LIST_X - 4, (int)y - 1, (int)SE_LIST_W + 8, (int)SE_LIST_ROW_H, (Color){42, 209, 86, 60});
        char line[96];
        snprintf(line, sizeof(line), "%d %s%s", idx, sprites[idx].image, sprites[idx].front ? " (F)" : "");
        SE_Text(line, SE_LIST_X, y, 18.0f, on ? GREEN : WHITE);
    }
    if (spriteCount == 0)
        SE_Text("vazio: Enter adiciona imagem", SE_LIST_X, SE_LIST_Y, 18.0f, GRAY);
}

static void Edit_DrawInfo(void)
{
    char line[256];
    if (sel >= 0 && sel < spriteCount) {
        const StageSprite *s = &sprites[sel];
        snprintf(line, sizeof(line), "%s  x=%.0f y=%.0f  escala=%.2f  scroll=%.1f,%.1f  %s",
                 s->image, s->x, s->y, s->scaleX, s->scrollX, s->scrollY, s->front ? "frente" : "atras");
    } else {
        snprintf(line, sizeof(line), "nenhum sprite selecionado");
    }
    SE_Text(line, 20.0f, 68.0f, 18.0f, WHITE);

    snprintf(line, sizeof(line), "stage: %s [%s]%s   zoom padrao=%.2f   camera=%.2fx",
             current.name, RootLabel(current.root), dirty ? " *nao salvo*" : "", meta.defaultZoom, camZoom);
    SE_Text(line, 20.0f, 92.0f - 2.0f, 16.0f, GRAY);
}

static void Edit_DrawHelp(void)
{
    SE_Text("mouse: arrasta sprite / dir. arrasta camera / roda escala   |   I J K L move (Shift x10)   "
            "Z/X C/V scroll   E frente   Del remove   Enter +imagem   -/= zoom   [ ] zoom padrao   Ctrl+S salvar   ESC sai",
            20.0f, 696.0f, 15.0f, GRAY);
}

static void Edit_Draw(void)
{
    SE_Text("STAGE EDITOR", 20.0f, 14.0f, 22.0f, WHITE);
    Edit_DrawInfo();
    Edit_DrawCanvas();
    Edit_DrawList();
    Panel_Draw();
    DrawStatus();
    Edit_DrawHelp();
}

static void Add_Draw(void)
{
    SE_Text("adicionar imagem", SE_LIST_X, SE_LIST_Y - 26.0f, 20.0f, YELLOW);
    SE_Text(addFilter, SE_LIST_X + 4.0f, SE_LIST_Y - 2.0f, 18.0f, WHITE);
    for (int row = 0; row < SE_LIST_ROWS; ++row) {
        const int idx = addTop + row;
        if (idx >= addVisibleCount)
            break;
        const float y = SE_LIST_Y + 22.0f + row * SE_LIST_ROW_H;
        const bool on = idx == addSel;
        if (on)
            DrawRectangle((int)SE_LIST_X - 4, (int)y - 1, (int)SE_LIST_W + 8, (int)SE_LIST_ROW_H, (Color){42, 209, 86, 60});
        SE_Text(imageNames[addVisible[idx]], SE_LIST_X, y, 18.0f, on ? GREEN : WHITE);
    }
    if (addVisibleCount == 0)
        SE_Text("nenhuma imagem em images/", SE_LIST_X, SE_LIST_Y + 22.0f, 18.0f, GRAY);
}

static void StageEditor_Draw([[maybe_unused]] RayScene *scene)
{
    BeginUi();
    DrawBackdrop();
    if (mode == SE_PICK) {
        Pick_Draw();
    } else {
        Edit_Draw();
        if (mode == SE_ADD)
            Add_Draw();
    }
    EndUi();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void StageEditor_Create([[maybe_unused]] RayScene *scene)
{
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = MenuBackground_Get();

    uiCam = (Camera2D){
        .target = {640.0f, 360.0f},
        .offset = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = 1.0f
    };

    rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    memset(sprites, 0, sizeof(sprites));
    memset(tex, 0, sizeof(tex));
    spriteCount = 0;
    sel = -1;
    listTop = 0;
    hasCurrent = false;
    dirty = false;
    escArmed = false;
    dragging = false;
    panning = false;
    camTarget = (Vector2){0.0f, 0.0f};
    camZoom = 0.5f;
    meta = (StageMeta){0};

    mode = SE_PICK;
    filter[0] = '\0';
    pickSel = 0;
    pickTop = 0;
    entryCount = StageEd_ScanList(entries, STAGEED_LIST_MAX);
    Rebuild_Pick();

    status[0] = '\0';
    statusTimer = 0.0f;
    if (rootCount == 0)
        SetStatus("nenhuma pasta de mod/assets encontrada");
}

static void StageEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    Textures_FreeAll();
    hasCurrent = false;
    bg.image = (Texture2D){0}; /* compartilhada: nao descarrega aqui */
}

static RayScene stageEditorScene;
Scene_MakeSceneCode(
    stageEditorScene,
    StageEditor_SetScene,
    StageEditor_Create,
    StageEditor_Update,
    StageEditor_Draw,
    StageEditor_Destroy
)
