/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Character Editor: escolhe um personagem (lista com busca), toca as animacoes num canvas com
 * ghost da idle para alinhar offsets e edita escala, flip, posicao, sing_duration, cor da barra de
 * vida e offset/fps/loop de cada animacao. Ctrl+S grava <raiz>/characters/<nome>.json.
 *
 * Canvas: esq. arrasta o offset da animacao | Ctrl+esq. arrasta a posicao | dir./meio move a vista
 *         roda = zoom
 * Teclas: W/S animacao | setas offset (Shift x10) | Ctrl+setas posicao | Espaco toca do inicio
 *         P pausa | , . frame | L loop | F flip | -/= escala | G ghost | C centraliza
 *         Ctrl+Z/Y desfaz/refaz | Ctrl+S salva | Esc volta a lista (pergunta se ha mudancas)
 */
#include "scenes/modding/charactereditor/CharEditor.h"
#include "scenes/modding/charactereditor/CharEditorIO.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/modding/ModdingMenu.h"
#include "characters/Character.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

#define ANIM_MAX   128
#define ANIM_ROW_H 30.0f
#define ROW_H      40.0f
#define UNDO_MAX   32

enum { CONFIRM_LEAVE = 1 };

typedef enum { CE_PICK = 0, CE_EDIT } CharEdMode;

static const Rectangle kPicker = {200.0f, 66.0f, 880.0f, 612.0f};
static const Rectangle kLeft = {16.0f, 62.0f, 280.0f, 620.0f};
static const Rectangle kList = {24.0f, 100.0f, 264.0f, 574.0f};
static const Rectangle kCanvas = {304.0f, 62.0f, 600.0f, 620.0f};
static const Rectangle kRight = {912.0f, 62.0f, 352.0f, 620.0f};
static const Rectangle kBtnSave = {1134.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnList = {994.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnCenter = {854.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnRescan = {1104.0f, 8.0f, 160.0f, 32.0f};
static const Rectangle kFlip = {1140.0f, 116.0f, 112.0f, 30.0f};   /* linha 1, ao lado da escala */

static float RowY(int row) { return 76.0f + (float)row * ROW_H; }
static Rectangle Half(int row, int col) { return (Rectangle){1020.0f + (float)col * 120.0f, RowY(row), 112.0f, 30.0f}; }
static Rectangle Third(int row, int col) { return (Rectangle){1020.0f + (float)col * 66.0f, RowY(row), 60.0f, 30.0f}; }
static Rectangle CheckAt(int row, int col) { return (Rectangle){924.0f + (float)col * 168.0f, RowY(row), 160.0f, 30.0f}; }
static Rectangle FrameBtn(int i) { return (Rectangle){1020.0f + (float)i * 80.0f, RowY(8), 72.0f, 30.0f}; }

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

/* O que vai para o JSON: base do desfazer e do "ha mudancas". Copiado so com memcpy. */
typedef struct {
    Vector2 offset[ANIM_MAX];
    int fps[ANIM_MAX];
    bool loop[ANIM_MAX];
    float scale;
    float posX, posY;     /* "position" do JSON (sem o psychShift) */
    float sing;
    int health[3];
    bool flip;
    bool hasHealth;
} CharDoc;

static CharEdMode mode;
static CharEdEntry entries[CHARED_LIST_MAX];
static int entryCount;
static MUiPicker picker;

static Character chara;
static bool hasChara;
static CharEdEntry current;
static MUiList animList;
static int animSel;

static CharDoc doc, committed, saved;
static CharDoc undoBuf[UNDO_MAX], redoBuf[UNDO_MAX];
static int undoCount, redoCount;

static bool paused;
static bool showGhost = true;

static Vector2 camTarget;
static float camZoom = 1.0f;
static int dragMode;          /* 0 nada, 1 offset, 2 posicao */
static Vector2 dragWorld0, dragStart;
static bool panning;
static Vector2 panLast;

/* ------------------------------------------------------------------------- */
/* Documento                                                                 */
/* ------------------------------------------------------------------------- */

static int AnimCount(void)
{
    const int n = hasChara ? chara.object.animationSet.animationCount : 0;
    return n < ANIM_MAX ? n : ANIM_MAX;
}

static RayAnimation *Anim(int i)
{
    return &chara.object.animationSet.animations[i];
}

static void DocFromChar(void)
{
    memset(&doc, 0, sizeof(doc));
    for (int i = 0; i < AnimCount(); ++i) {
        doc.offset[i] = Anim(i)->animationOffset;
        doc.fps[i] = Anim(i)->fps > 0 ? Anim(i)->fps : 24;
        doc.loop[i] = Anim(i)->looped != 0;
    }
    doc.scale = fabsf(chara.object.scaleY) > 0.0001f ? fabsf(chara.object.scaleY) : 1.0f;
    doc.flip = chara.object.scaleX < 0.0f;
    doc.posX = chara.object.position.x - chara.psychShift.x;
    doc.posY = chara.object.position.y - chara.psychShift.y;
    doc.sing = chara.singDuration;
    doc.hasHealth = chara.hasHealthColor;
    doc.health[0] = chara.healthColor.r;
    doc.health[1] = chara.healthColor.g;
    doc.health[2] = chara.healthColor.b;
}

/* Todo frame: o personagem mostra exatamente o documento. */
static void DocToChar(void)
{
    if (!hasChara)
        return;
    RayAnimationHandler *set = &chara.object.animationSet;
    for (int i = 0; i < AnimCount(); ++i) {
        set->animations[i].animationOffset = doc.offset[i];
        if (set->animations[i].fps != doc.fps[i] || (set->animations[i].looped != 0) != doc.loop[i])
            AnimationSet_SetAnimationData(set, i, doc.fps[i], doc.loop[i] ? 1 : 0);
    }
    chara.object.scaleY = doc.scale;
    chara.object.scaleX = doc.flip ? -doc.scale : doc.scale;
    chara.object.position = (Vector2){doc.posX + chara.psychShift.x, doc.posY + chara.psychShift.y};
    chara.singDuration = doc.sing;
    chara.hasHealthColor = doc.hasHealth;
    chara.healthColor = (Color){(unsigned char)doc.health[0], (unsigned char)doc.health[1], (unsigned char)doc.health[2], 255};
}

static bool Dirty(void)
{
    return hasChara && memcmp(&doc, &saved, sizeof(doc)) != 0;
}

static void ResetHistory(void)
{
    memcpy(&committed, &doc, sizeof(doc));
    memcpy(&saved, &doc, sizeof(doc));
    undoCount = redoCount = 0;
}

/* Guarda o estado anterior quando algo mudou e nenhum gesto esta em andamento. */
static void CommitUndo(void)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || MUi_NumBusy() || MUi_TextActive() ||
        memcmp(&doc, &committed, sizeof(doc)) == 0)
        return;
    if (undoCount == UNDO_MAX) {
        memmove(&undoBuf[0], &undoBuf[1], sizeof(CharDoc) * (UNDO_MAX - 1));
        --undoCount;
    }
    memcpy(&undoBuf[undoCount++], &committed, sizeof(doc));
    memcpy(&committed, &doc, sizeof(doc));
    redoCount = 0;
}

static void Undo(bool redo)
{
    CharDoc *from = redo ? redoBuf : undoBuf;
    CharDoc *to = redo ? undoBuf : redoBuf;
    int *fromN = redo ? &redoCount : &undoCount;
    int *toN = redo ? &undoCount : &redoCount;
    if (*fromN == 0) {
        MUi_SetStatus(MUI_INFO, redo ? "nothing to redo" : "nothing to undo");
        return;
    }
    if (*toN == UNDO_MAX) {
        memmove(&to[0], &to[1], sizeof(CharDoc) * (UNDO_MAX - 1));
        --*toN;
    }
    memcpy(&to[(*toN)++], &doc, sizeof(doc));
    memcpy(&doc, &from[--*fromN], sizeof(doc));
    memcpy(&committed, &doc, sizeof(doc));
    MUi_SetStatus(MUI_INFO, redo ? "refeito" : "desfeito");
}

/* ------------------------------------------------------------------------- */
/* Animacao e vista                                                          */
/* ------------------------------------------------------------------------- */

static void Play(void)
{
    if (animSel >= 0 && animSel < AnimCount()) {
        AnimatedObject_SetAnimation(&chara.object, animSel);
        paused = false;
    }
}

static void SelectAnim(int i)
{
    if (i < 0 || i >= AnimCount())
        return;
    animSel = i;
    animList.sel = i;
    MUi_ListShow(&animList, kList, ANIM_ROW_H, AnimCount());
    Play();
}

static void StepFrame(int delta)
{
    const RayAnimation *a = Anim(chara.object.currentAnim.animationIndex);
    if (a->frameCount <= 0)
        return;
    paused = true;
    const int n = a->frameCount;
    const int cur = (int)chara.object.currentAnim.currentFrame;
    chara.object.currentAnim.currentFrame = ((cur + delta) % n + n) % n;
    chara.object.currentAnim.time = 0.0f;
}

static Vector2 ScreenToWorld(Vector2 p)
{
    return (Vector2){camTarget.x + (p.x - (kCanvas.x + kCanvas.width * 0.5f)) / camZoom,
                     camTarget.y + (p.y - (kCanvas.y + kCanvas.height * 0.5f)) / camZoom};
}

/* Caixa do frame atual no mundo (mesma conta do Render_DrawAnimatedObject). */
static bool FrameRect(int anim, unsigned frame, Rectangle *out)
{
    if (anim < 0 || anim >= chara.object.animationSet.animationCount)
        return false;
    const RayAnimation *a = Anim(anim);
    if (!a->frames || a->frameCount <= 0)
        return false;
    const Frame *f = &a->frames[frame < (unsigned)a->frameCount ? frame : 0];
    const float sx = fabsf(chara.object.scaleX), sy = chara.object.scaleY;
    *out = (Rectangle){chara.object.position.x - a->animationOffset.x - (float)f->fx * sx,
                       chara.object.position.y - a->animationOffset.y - (float)f->fy * sy,
                       (float)f->w * sx, (float)f->h * sy};
    return true;
}

static void CenterView(void)
{
    Rectangle r;
    camZoom = 1.0f;
    if (hasChara && FrameRect(chara.animations.idle >= 0 ? chara.animations.idle : 0, 0, &r)) {
        camTarget = (Vector2){r.x + r.width * 0.5f, r.y + r.height * 0.5f};
        camZoom = fminf(fminf(kCanvas.width / (r.width + 120.0f), kCanvas.height / (r.height + 120.0f)), 2.0f);
    } else {
        camTarget = (Vector2){0.0f, 0.0f};
    }
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar                                                            */
/* ------------------------------------------------------------------------- */

static const char *CharLabel(int i, int col)
{
    if (col == 0)
        return entries[i].name;
    return col == 2 ? ModEd_RootLabel(entries[i].root) : NULL;
}

static void Rescan(void)
{
    entryCount = CharEd_ScanList(entries, CHARED_LIST_MAX);
    MUi_PickerFilter(&picker, entryCount, CharLabel);
}

static void CloseCharacter(void)
{
    if (hasChara)
        CharEd_FreeCharacter(&chara);
    hasChara = false;
    memset(&doc, 0, sizeof(doc));
    ResetHistory();
    MUi_TextBlur();
    mode = CE_PICK;
    Rescan();
}

static void OpenEntry(const CharEdEntry *entry)
{
    if (!CharEd_LoadCharacter(entry, &chara)) {
        MUi_SetStatus(MUI_ERR, "could not load %s", entry->name);
        return;
    }
    current = *entry;
    hasChara = true;
    DocFromChar();
    ResetHistory();
    animList = (MUiList){0};
    animSel = 0;
    mode = CE_EDIT;
    SelectAnim(chara.animations.idle >= 0 ? chara.animations.idle : 0);
    CenterView();
    if (chara.object.animationSet.animationCount > ANIM_MAX)
        MUi_SetStatus(MUI_WARN, "%d animations: only the first %d are editable", chara.object.animationSet.animationCount, ANIM_MAX);
    else
        MUi_SetStatus(MUI_OK, "%s: %d animacoes", entry->name, AnimCount());
}

static bool SaveCurrent(void)
{
    char msg[220] = {0};
    MUi_TextBlur();
    DocToChar();
    if (!CharEd_Save(&current, &chara, msg, sizeof(msg))) {
        MUi_SetStatus(MUI_ERR, "%s", msg[0] ? msg : "save failed");
        return false;
    }
    memcpy(&saved, &doc, sizeof(doc));
    MUi_SetStatus(MUI_OK, "%s", msg);
    return true;
}

static void AskLeave(void)
{
    if (Dirty())
        MUi_ConfirmOpen(CONFIRM_LEAVE, "Back to list", "There are unsaved changes in this character.");
    else
        CloseCharacter();
}

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

static void Pick_Update(void)
{
    if (MUi_Button(kBtnRescan) || IsKeyPressed(KEY_F5)) {
        Rescan();
        MUi_SetStatus(MUI_INFO, "%d characters", entryCount);
    }
    const int pick = MUi_PickerUpdate(&picker, kPicker, entryCount, CharLabel);
    if (pick >= 0)
        OpenEntry(&entries[pick]);
    else if (pick == MUI_PICK_BACK)
        ModdingMenu_SetScene();
}

static void Panel_Update(void)
{
    MUi_Num(Half(1, 0), &doc.scale, 0.01f, 0.05f, 10.0f);
    MUi_Check(kFlip, &doc.flip);
    MUi_Num(Half(2, 0), &doc.posX, 1.0f, -10000.0f, 10000.0f);
    MUi_Num(Half(2, 1), &doc.posY, 1.0f, -10000.0f, 10000.0f);
    MUi_Num(Half(3, 0), &doc.sing, 0.1f, 0.1f, 100.0f);
    for (int i = 0; i < 3; ++i) {
        if (MUi_NumInt(Third(4, i), &doc.health[i], 1, 0, 255))
            doc.hasHealth = true;
    }

    if (animSel < 0 || animSel >= AnimCount())
        return;
    MUi_Num(Half(6, 0), &doc.offset[animSel].x, 1.0f, -10000.0f, 10000.0f);
    MUi_Num(Half(6, 1), &doc.offset[animSel].y, 1.0f, -10000.0f, 10000.0f);
    MUi_NumInt(Half(7, 0), &doc.fps[animSel], 1, 1, 240);
    MUi_Check(CheckAt(7, 1), &doc.loop[animSel]);
    if (MUi_Button(FrameBtn(0))) StepFrame(-1);
    if (MUi_Button(FrameBtn(1))) {
        if (paused) paused = false;
        else Play();
    }
    if (MUi_Button(FrameBtn(2))) StepFrame(1);
    if (MUi_Button(FrameBtn(3))) paused = !paused;
    MUi_Check(CheckAt(9, 0), &showGhost);
}

static void Canvas_Update(void)
{
    const Vector2 m = MUi_Mouse();
    const bool over = MUi_Hover(kCanvas) && MUi_MouseFree();

    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && animSel < AnimCount()) {
        MUi_TakeMouse();
        MUi_TextBlur();
        dragMode = MUi_Ctrl() ? 2 : 1;
        dragWorld0 = ScreenToWorld(m);
        dragStart = dragMode == 2 ? (Vector2){doc.posX, doc.posY} : doc.offset[animSel];
    }
    if (dragMode) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const Vector2 w = ScreenToWorld(m);
            const float dx = roundf(w.x - dragWorld0.x), dy = roundf(w.y - dragWorld0.y);
            if (dragMode == 2) {
                doc.posX = dragStart.x + dx;
                doc.posY = dragStart.y + dy;
            } else {
                /* destino = posicao - offset: arrastar para a direita diminui o offset */
                doc.offset[animSel] = (Vector2){dragStart.x - dx, dragStart.y - dy};
            }
        } else {
            dragMode = 0;
        }
    }

    if (over && (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))) {
        MUi_TakeMouse();
        panning = true;
        panLast = m;
    }
    if (panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            camTarget.x -= (m.x - panLast.x) / camZoom;
            camTarget.y -= (m.y - panLast.y) / camZoom;
            panLast = m;
        } else {
            panning = false;
        }
    }

    const float wheel = GetMouseWheelMove();
    if (MUi_Hover(kCanvas) && wheel != 0.0f) {
        const Vector2 before = ScreenToWorld(m);
        camZoom = fminf(fmaxf(camZoom * (wheel > 0.0f ? 1.15f : 1.0f / 1.15f), 0.1f), 6.0f);
        const Vector2 after = ScreenToWorld(m);
        camTarget.x += before.x - after.x;
        camTarget.y += before.y - after.y;
    }
}

static void Keys_Update(void)
{
    const int count = AnimCount();
    const float step = MUi_Shift() ? 10.0f : 1.0f;
    const bool hasAnim = animSel >= 0 && animSel < count;

    if (MUi_Ctrl()) {
        if (IsKeyPressed(KEY_S)) SaveCurrent();
        if (MUi_Rep(KEY_Z)) Undo(MUi_Shift());
        if (MUi_Rep(KEY_Y)) Undo(true);
        if (MUi_Rep(KEY_LEFT)) doc.posX -= step;
        if (MUi_Rep(KEY_RIGHT)) doc.posX += step;
        if (MUi_Rep(KEY_UP)) doc.posY -= step;
        if (MUi_Rep(KEY_DOWN)) doc.posY += step;
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        AskLeave();
        return;
    }
    if (count > 0) {
        if (MUi_Rep(KEY_S)) SelectAnim((animSel + 1) % count);
        if (MUi_Rep(KEY_W)) SelectAnim((animSel + count - 1) % count);
    }
    if (hasAnim) {
        /* setas = offset (Psych): o sprite anda para o lado da seta */
        if (MUi_Rep(KEY_LEFT)) doc.offset[animSel].x += step;
        if (MUi_Rep(KEY_RIGHT)) doc.offset[animSel].x -= step;
        if (MUi_Rep(KEY_UP)) doc.offset[animSel].y += step;
        if (MUi_Rep(KEY_DOWN)) doc.offset[animSel].y -= step;
        if (IsKeyPressed(KEY_L)) doc.loop[animSel] = !doc.loop[animSel];
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) Play();
    if (IsKeyPressed(KEY_P)) paused = !paused;
    if (MUi_Rep(KEY_COMMA)) StepFrame(-1);
    if (MUi_Rep(KEY_PERIOD)) StepFrame(1);
    if (IsKeyPressed(KEY_F)) doc.flip = !doc.flip;
    if (IsKeyPressed(KEY_G)) showGhost = !showGhost;
    if (IsKeyPressed(KEY_C)) CenterView();
    if (MUi_Rep(KEY_MINUS) || MUi_Rep(KEY_KP_SUBTRACT)) doc.scale = fmaxf(doc.scale - 0.05f, 0.05f);
    if (MUi_Rep(KEY_EQUAL) || MUi_Rep(KEY_KP_ADD)) doc.scale = fminf(doc.scale + 0.05f, 10.0f);
}

static void Edit_Update(void)
{
    if (MUi_Button(kBtnSave)) SaveCurrent();
    if (MUi_Button(kBtnCenter)) CenterView();
    if (MUi_Button(kBtnList)) {
        AskLeave();
        return;
    }

    const int clicked = MUi_List(&animList, kList, ANIM_ROW_H, AnimCount(), NULL);
    if (clicked >= 0)
        SelectAnim(clicked);
    Panel_Update();
    Canvas_Update();
    if (MUi_KeysFree())
        Keys_Update();

    DocToChar();
    if (!paused)
        AnimatedObject_UpdateFrame(&chara.object);
    CommitUndo();
}

static void CharEditor_Update([[maybe_unused]] RayScene *scene)
{
    MUi_FrameBegin();
    MUi_Tick(RayGame_DeltaTime());

    if (MUi_ModalActive()) {
        int id = 0;
        const MUiModalResult res = MUi_ModalUpdate(&id);
        if (id == CONFIRM_LEAVE && (res == MUI_MODAL_DISCARD || (res == MUI_MODAL_OK && SaveCurrent())))
            CloseCharacter();
        return;
    }
    if (MUi_HelpUpdate())
        return;
    if (mode == CE_PICK)
        Pick_Update();
    else
        Edit_Update();
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

static void DrawCharFrame(int anim, unsigned frame, Color tint)
{
    Rectangle dst;
    if (!FrameRect(anim, frame, &dst))
        return;
    const RayAnimation *a = Anim(anim);
    const Frame *f = &a->frames[frame < (unsigned)a->frameCount ? frame : 0];
    const bool flip = chara.object.scaleX < 0.0f;
    const Rectangle src = {(float)f->x, (float)f->y, flip ? -(float)f->w : (float)f->w, (float)f->h};
    DrawTexturePro(chara.object.animationSet.texture, src, dst, (Vector2){0}, 0.0f, tint);
}

static void DrawCanvas(void)
{
    DrawRectangleRec(kCanvas, (Color){6, 4, 14, 255});
    MUi_ClipBegin(kCanvas);
    Camera2D cam = {0};
    cam.offset = (Vector2){kCanvas.x + kCanvas.width * 0.5f, kCanvas.y + kCanvas.height * 0.5f};
    cam.target = camTarget;
    cam.zoom = camZoom;
    BeginMode2D(cam);

    /* eixo na posicao do personagem */
    const Vector2 o = chara.object.position;
    const float lw = 1.0f / camZoom;
    DrawLineEx((Vector2){o.x - 4000.0f, o.y}, (Vector2){o.x + 4000.0f, o.y}, lw, (Color){255, 255, 255, 30});
    DrawLineEx((Vector2){o.x, o.y - 4000.0f}, (Vector2){o.x, o.y + 4000.0f}, lw, (Color){255, 255, 255, 30});

    const int idle = chara.animations.idle >= 0 ? chara.animations.idle : 0;
    const int cur = chara.object.currentAnim.isValid ? chara.object.currentAnim.animationIndex : animSel;
    if (showGhost && cur != idle)
        DrawCharFrame(idle, 0, (Color){120, 180, 255, 90});
    DrawCharFrame(cur, chara.object.currentAnim.currentFrame, chara.object.color);

    Rectangle r;
    if (FrameRect(cur, chara.object.currentAnim.currentFrame, &r))
        DrawRectangleLinesEx(r, 1.5f / camZoom, ColorAlpha(MUI_C_ACCENT, 0.6f));
    EndMode2D();
    MUi_ClipEnd();

    char line[96];
    snprintf(line, sizeof(line), "view %d%%%s", (int)(camZoom * 100.0f + 0.5f), paused ? "   PAUSED" : "");
    MUi_TextShadow(line, kCanvas.x + 8.0f, kCanvas.y + kCanvas.height - 24.0f, 15.0f, paused ? MUI_C_HINT : MUI_C_DIM);
    DrawRectangleLinesEx(kCanvas, 1.0f, MUI_C_EDGE);
}

static void DrawAnimList(void)
{
    MUi_DrawPanel(kLeft);
    MUi_DrawSection(kLeft.x + 10.0f, kLeft.y + 10.0f, "Animations");
    const int count = AnimCount();
    MUi_DrawListBegin(kList);
    const int rows = MUi_ListRows(kList, ANIM_ROW_H);
    for (int i = animList.top; i < count && i <= animList.top + rows; ++i) {
        const Rectangle row = MUi_ListRow(&animList, kList, ANIM_ROW_H, i);
        MUi_DrawListRow(row, i == animSel);
        MUi_ClipBegin((Rectangle){row.x, row.y, row.width - 70.0f, row.height});
        MUi_Text(Anim(i)->name, row.x + 8.0f, row.y + 6.0f, 17.0f, i == animSel ? MUI_C_TEXT : MUI_C_DIM);
        MUi_ClipEnd();
        char off[32];
        snprintf(off, sizeof(off), "%.0f,%.0f%s", doc.offset[i].x, doc.offset[i].y, doc.loop[i] ? " L" : "");
        MUi_TextRight(off, row.x + row.width - 8.0f, row.y + 8.0f, 14.0f, doc.loop[i] ? MUI_C_HINT : MUI_C_FAINT);
    }
    if (count == 0)
        MUi_TextCenter("no animations", (Rectangle){kList.x, kList.y + 10.0f, kList.width, 24.0f}, 16.0f, MUI_C_FAINT);
    MUi_DrawListEnd(&animList, kList, ANIM_ROW_H, count);
}

static void Label(int row, const char *text)
{
    MUi_Text(text, 924.0f, RowY(row) + 6.0f, 17.0f, MUI_C_DIM);
}

static void DrawPanel(void)
{
    MUi_DrawPanel(kRight);
    MUi_DrawSection(924.0f, RowY(0) + 4.0f, "Character");
    Label(1, "Scale");
    MUi_DrawNum(Half(1, 0), doc.scale, 0.01f);
    MUi_DrawCheck(kFlip, "Flip (F)", doc.flip);
    Label(2, "Position");
    MUi_DrawNum(Half(2, 0), doc.posX, 1.0f);
    MUi_DrawNum(Half(2, 1), doc.posY, 1.0f);
    Label(3, "Sing dur.");
    MUi_DrawNum(Half(3, 0), doc.sing, 0.1f);
    MUi_Text("steps", 1140.0f, RowY(3) + 7.0f, 15.0f, MUI_C_FAINT);
    Label(4, "Health color");
    for (int i = 0; i < 3; ++i)
        MUi_DrawNum(Third(4, i), (float)doc.health[i], 1.0f);
    MUi_DrawSwatch((Rectangle){1218.0f, RowY(4), 34.0f, 30.0f},
                   (Color){(unsigned char)doc.health[0], (unsigned char)doc.health[1], (unsigned char)doc.health[2], 255}, false);

    if (animSel >= 0 && animSel < AnimCount()) {
        char title[64];
        snprintf(title, sizeof(title), "Animation: %s", Anim(animSel)->name);
        MUi_DrawSection(924.0f, RowY(5) + 10.0f, title);
        Label(6, "Offset");
        MUi_DrawNum(Half(6, 0), doc.offset[animSel].x, 1.0f);
        MUi_DrawNum(Half(6, 1), doc.offset[animSel].y, 1.0f);
        Label(7, "FPS");
        MUi_DrawNum(Half(7, 0), (float)doc.fps[animSel], 1.0f);
        MUi_DrawCheck(CheckAt(7, 1), "Loop (L)", doc.loop[animSel]);

        const RayAnimation *a = Anim(chara.object.currentAnim.animationIndex);
        char frame[32];
        snprintf(frame, sizeof(frame), "Frame %u/%d", chara.object.currentAnim.currentFrame + 1, a->frameCount);
        Label(8, frame);
        MUi_DrawButton(FrameBtn(0), "<", MUI_BTN_NORMAL);
        MUi_DrawButton(FrameBtn(1), "Play", MUI_BTN_NORMAL);
        MUi_DrawButton(FrameBtn(2), ">", MUI_BTN_NORMAL);
        MUi_DrawButton(FrameBtn(3), "Pause", paused ? MUI_BTN_ACTIVE : MUI_BTN_NORMAL);
        MUi_DrawCheck(CheckAt(9, 0), "Idle ghost (G)", showGhost);
    }

    /* icone da barra de vida */
    if (chara.icon.id) {
        const Rectangle src = Character_IconFrame(chara.icon);
        const Rectangle dst = {924.0f, kRight.y + kRight.height - 118.0f, 100.0f, 100.0f};
        DrawTexturePro(chara.icon, src, dst, (Vector2){0}, 0.0f, WHITE);
        DrawRectangleRec((Rectangle){1032.0f, dst.y + 40.0f, 220.0f, 18.0f},
                         (Color){(unsigned char)doc.health[0], (unsigned char)doc.health[1], (unsigned char)doc.health[2], 255});
        MUi_Text(doc.hasHealth ? "health bar" : "health bar (default)", 1032.0f, dst.y + 64.0f, 15.0f, MUI_C_FAINT);
    }
}

static void Edit_Draw(void)
{
    char detail[CHARED_NAME_MAX + CHARED_ROOT_MAX + 8];
    snprintf(detail, sizeof(detail), "%s  [%s]", current.name, ModEd_RootLabel(current.root));
    MUi_DrawTopBar("Character Editor", detail, Dirty());
    MUi_DrawButton(kBtnCenter, "Center (C)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnList, "List (Esc)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnSave, "Save", Dirty() ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);
    DrawCanvas();
    DrawAnimList();
    DrawPanel();
    MUi_DrawBottomBar();
}

static const MUiHelpRow kPickHelp[] = {
    {"type", "search character"},
    {"Enter / click", "open character"},
    {"F5", "refresh list"},
    {"Esc", "return to editor menu"},
};

static const MUiHelpRow kEditHelp[] = {
    {"W / S", "previous / next animation"},
    {"Arrows", "offset (Shift x10)"},
    {"Ctrl+Arrows", "position"},
    {"Drag", "offset (Ctrl: position)"},
    {"Space", "play from start"},
    {"P", "pause"},
    {", / .", "previous / next frame"},
    {"L / F / G", "loop / flip / idle ghost"},
    {"- / =", "scale"},
    {"C", "center view"},
    {"Ctrl+Z / Y", "undo / redo"},
    {"Ctrl+S", "save"},
    {"Esc", "return to character list"},
    {"Wheel", "zoom view"},
};

static void CharEditor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    if (mode == CE_PICK) {
        MUi_DrawTopBar("Character Editor", "choose a character", false);
        MUi_DrawButton(kBtnRescan, "Refresh (F5)", MUI_BTN_NORMAL);
        MUi_DrawPicker(&picker, kPicker, CharLabel, "search character...",
                       entryCount ? "no character with that name" : "no character found (.animset + png)");
        MUi_DrawBottomBar();
    } else {
        Edit_Draw();
    }
    if (mode == CE_PICK)
        MUi_DrawHelp("Character Editor - list", kPickHelp, (int)(sizeof(kPickHelp) / sizeof(kPickHelp[0])));
    else
        MUi_DrawHelp("Character Editor - shortcuts", kEditHelp, (int)(sizeof(kEditHelp) / sizeof(kEditHelp[0])));
    MUi_DrawModal();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void CharEditor_Create([[maybe_unused]] RayScene *scene)
{
    MUi_Reset();
    memset(&picker, 0, sizeof(picker));
    memset(&current, 0, sizeof(current));
    hasChara = false;
    paused = false;
    dragMode = 0;
    panning = false;
    memset(&doc, 0, sizeof(doc));
    ResetHistory();
    mode = CE_PICK;
    Rescan();
}

static void CharEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    if (hasChara)
        CharEd_FreeCharacter(&chara);
    hasChara = false;
    MUi_Reset();
}

static RayScene characterEditorScene;
Scene_MakeSceneCode(
    characterEditorScene,
    CharEditor_SetScene,
    CharEditor_Create,
    CharEditor_Update,
    CharEditor_Draw,
    CharEditor_Destroy
)
