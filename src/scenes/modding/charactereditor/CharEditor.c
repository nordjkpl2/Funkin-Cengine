/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Character Editor: escolhe um personagem, toca as animacoes no palco e ajusta offset, escala,
 * flip, posicao e loop. Ctrl+S grava <mod>/characters/<nome>.json.
 */
#include "scenes/modding/charactereditor/CharEditor.h"
#include "scenes/modding/charactereditor/CharEditorIO.h"
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "characters/Character.h"
#include "Game.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout (canvas 1280x720)                                                  */
/* ------------------------------------------------------------------------- */

#define PICK_X      60.0f
#define PICK_Y      130.0f
#define PICK_ROW_H  28.0f
#define PICK_ROWS   19
#define PICK_W      820.0f

#define ANIM_X      20.0f
#define ANIM_Y      130.0f
#define ANIM_W      260.0f
#define ANIM_ROW_H  22.0f
#define ANIM_ROWS   20

#define INFO_X      960.0f
#define INFO_Y      90.0f
#define INFO_W      300.0f

#define STAGE_X     760.0f   /* origem (0,0) do personagem na tela */
#define STAGE_Y     520.0f

#define SCALE_MIN   0.05f
#define SCALE_MAX   8.0f
#define SCALE_STEP  0.05f

#define ZOOM_MIN    0.25f
#define ZOOM_MAX    4.0f
#define ZOOM_STEP   1.25f

#define FILTER_MAX  48

/* botoes de zoom (canvas): - | 100% | + */
#define ZOOM_OUT_X  1000.0f
#define ZOOM_RESET_X 1070.0f
#define ZOOM_IN_X   1180.0f
#define ZOOM_BTN_Y  400.0f

/* botoes de direcao (setas de pan) */
#define PAN_UP_X    1110.0f
#define PAN_LEFT_X  1040.0f
#define PAN_RIGHT_X 1180.0f
#define PAN_SIDE_Y  500.0f
#define PAN_DOWN_Y  550.0f
#define PAN_UP_Y    450.0f

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

typedef enum {
    CE_PICK = 0,
    CE_EDIT
} CharEdMode;

static RayGraphicObject bg;
static Camera2D uiCam;
static Camera2D stageCam;

static CharEdMode mode;
static CharEdEntry entries[CHARED_LIST_MAX];
static int entryCount;
static int pickSel;
static int pickTop;
static float pickWheel;

static Character chara;
static bool hasChara;
static CharEdEntry current;
static int animSel;
static int animTop;
static float animWheel;
static bool dirty;
static bool escArmed;

static char status[192];
static float statusTimer;

/* busca da lista: visible[] guarda os indices de entries[] que batem com o filtro */
static char filter[FILTER_MAX];
static int filterLen;
static int visible[CHARED_LIST_MAX];
static int visibleCount;

static float stageZoom = 1.0f;
static Vector2 stagePan;      /* deslocamento do palco em pixels do canvas */
static bool panning;
static Vector2 panLast;

static void CE_Text(const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(mainFont, text, (Vector2){x, y}, size, 1.0f, color);
}

static void SetStatus(const char *text)
{
    snprintf(status, sizeof(status), "%s", text);
    statusTimer = 4.0f;
}

static const char *RootLabel(const char *root)
{
    const char *slash = strrchr(root, '/');
    const char *back = strrchr(root, '\\');
    if (back && (!slash || back > slash))
        slash = back;
    return slash ? slash + 1 : root;
}

static int ClampInt(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

/* Busca sem diferenciar maiusculas: needle dentro de hay. */
static bool ContainsCI(const char *hay, const char *needle)
{
    const size_t n = strlen(needle);
    if (n == 0)
        return true;
    for (const char *p = hay; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == n)
            return true;
    }
    return false;
}

static void RebuildVisible(void)
{
    visibleCount = 0;
    for (int i = 0; i < entryCount && visibleCount < CHARED_LIST_MAX; ++i) {
        if (ContainsCI(entries[i].name, filter))
            visible[visibleCount++] = i;
    }
}

static void FilterChanged(void)
{
    RebuildVisible();
    pickSel = 0;
    pickTop = 0;
}

static void AppendFilterChar(int c)
{
    if (c < 32 || c > 126 || filterLen >= FILTER_MAX - 1)
        return;
    filter[filterLen++] = (char)c;
    filter[filterLen] = '\0';
    FilterChanged();
}

/* Cola o texto da area de transferencia: so caracteres imprimiveis, ate a primeira quebra de linha. */
static void PasteFilter(const char *text)
{
    if (!text)
        return;
    for (const char *p = text; *p && *p != '\n' && *p != '\r' && filterLen < FILTER_MAX - 1; ++p) {
        if (*p >= 32 && *p <= 126)
            filter[filterLen++] = *p;
    }
    filter[filterLen] = '\0';
    FilterChanged();
}

/* Mantem a selecao visivel na janela de linhas (lista filtrada). */
static void ClampPickView(void)
{
    pickSel = visibleCount > 0 ? ClampInt(pickSel, 0, visibleCount - 1) : 0;
    if (pickSel < pickTop)
        pickTop = pickSel;
    if (pickSel >= pickTop + PICK_ROWS)
        pickTop = pickSel - PICK_ROWS + 1;
    pickTop = ClampInt(pickTop, 0, visibleCount > PICK_ROWS ? visibleCount - PICK_ROWS : 0);
}

/* Zoom do palco. Render_SetCamera poe o target no centro do canvas, entao o target tem de
   acompanhar o zoom para a origem do personagem continuar em (STAGE_X, STAGE_Y). */
static void ApplyStageZoom(float zoom)
{
    stageZoom = zoom < ZOOM_MIN ? ZOOM_MIN : (zoom > ZOOM_MAX ? ZOOM_MAX : zoom);
    stageCam.zoom = stageZoom;
    stageCam.target = (Vector2){(640.0f - (STAGE_X + stagePan.x)) / stageZoom,
                                (360.0f - (STAGE_Y + stagePan.y)) / stageZoom};
}

static bool RectHover(Rectangle r)
{
    const Vector2 m = RayGame_MouseWorld();
    return m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height;
}

static bool RectClicked(Rectangle r)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && RectHover(r);
}

static int AnimCount(void)
{
    return hasChara ? chara.object.animationSet.animationCount : 0;
}

static void ClampAnimView(void)
{
    const int count = AnimCount();
    animSel = count > 0 ? ClampInt(animSel, 0, count - 1) : 0;
    if (animSel < animTop)
        animTop = animSel;
    if (animSel >= animTop + ANIM_ROWS)
        animTop = animSel - ANIM_ROWS + 1;
    animTop = ClampInt(animTop, 0, count > ANIM_ROWS ? count - ANIM_ROWS : 0);
}

static void PlaySelected(void)
{
    if (hasChara && animSel >= 0 && animSel < AnimCount())
        AnimatedObject_SetAnimation(&chara.object, animSel);
}

static void SelectAnim(int index)
{
    if (index < 0 || index >= AnimCount())
        return;
    animSel = index;
    ClampAnimView();
    PlaySelected();
}

static void CloseCharacter(void)
{
    if (hasChara)
        CharEd_FreeCharacter(&chara);
    hasChara = false;
    dirty = false;
    escArmed = false;
    mode = CE_PICK;
    ClampPickView();
}

static void OpenEntry(int index)
{
    if (index < 0 || index >= entryCount)
        return;

    if (!CharEd_LoadCharacter(&entries[index], &chara)) {
        SetStatus("nao consegui carregar o personagem");
        return;
    }

    current = entries[index];
    hasChara = true;
    dirty = false;
    escArmed = false;
    animSel = 0;
    animTop = 0;
    mode = CE_EDIT;
    ClampAnimView();
    PlaySelected();
    SetStatus("W/S escolhe anim  ESPACO toca  setas: offset  Ctrl+setas: posicao  Ctrl+S salva");
}

static void SaveCurrent(void)
{
    char msg[220];
    const bool ok = CharEd_Save(&current, &chara, msg, sizeof(msg));
    if (ok)
        dirty = false;
    escArmed = false;
    SetStatus(msg);
}

static void NudgeOffset(float dx, float dy)
{
    if (!hasChara || animSel < 0 || animSel >= AnimCount())
        return;
    RayAnimationHandler *set = &chara.object.animationSet;
    const Vector2 offset = set->animations[animSel].animationOffset;
    AnimationSet_SetAnimationOffset(set, animSel, (Vector2){offset.x + dx, offset.y + dy});
    dirty = true;
}

static void ApplyScale(float scale)
{
    const bool flip = chara.object.scaleX < 0.0f;
    chara.object.scaleY = scale;
    chara.object.scaleX = flip ? -scale : scale;
    dirty = true;
}

/* ------------------------------------------------------------------------- */
/* Campos numericos: arrastar muda o valor, clicar sem arrastar abre digitacao */
/* ------------------------------------------------------------------------- */

typedef enum {
    FLD_SCALE = 0,
    FLD_POS_X,
    FLD_POS_Y,
    FLD_OFF_X,
    FLD_OFF_Y,
    FLD_FPS,
    FLD_COUNT
} FieldId;

static int activeField = -1;
static bool fieldDragging;
static bool fieldDragMoved;
static bool fieldEditing;
static float fieldDragStartMouse;
static float fieldDragStartValue;
static char fieldBuf[16];
static int fieldBufLen;

static float ClampF(float v, float low, float high)
{
    return v < low ? low : (v > high ? high : v);
}

static Rectangle FieldRect(int id)
{
    switch (id) {
        case FLD_SCALE: return (Rectangle){1100.0f, 98.0f, 100.0f, 26.0f};
        case FLD_POS_X: return (Rectangle){1026.0f, 164.0f, 90.0f, 26.0f};
        case FLD_POS_Y: return (Rectangle){1140.0f, 164.0f, 90.0f, 26.0f};
        case FLD_OFF_X: return (Rectangle){1026.0f, 268.0f, 90.0f, 26.0f};
        case FLD_FPS:   return (Rectangle){1026.0f, 318.0f, 90.0f, 26.0f};
        default:        return (Rectangle){1140.0f, 268.0f, 90.0f, 26.0f};
    }
}

static float FieldStep(int id)
{
    return id == FLD_SCALE ? 0.01f : 1.0f;
}

static float FieldValue(int id)
{
    if (!hasChara)
        return 0.0f;
    const RayAnimatedObject *o = &chara.object;
    const bool validAnim = animSel >= 0 && animSel < AnimCount();
    switch (id) {
        case FLD_SCALE: return fabsf(o->scaleY);
        case FLD_POS_X: return o->position.x;
        case FLD_POS_Y: return o->position.y;
        case FLD_OFF_X: return validAnim ? o->animationSet.animations[animSel].animationOffset.x : 0.0f;
        case FLD_FPS:   return validAnim ? (float)o->animationSet.animations[animSel].fps : 0.0f;
        default:        return validAnim ? o->animationSet.animations[animSel].animationOffset.y : 0.0f;
    }
}

static void FieldApply(int id, float v)
{
    if (!hasChara)
        return;
    RayAnimatedObject *o = &chara.object;
    const bool validAnim = animSel >= 0 && animSel < AnimCount();
    switch (id) {
        case FLD_SCALE:
            ApplyScale(ClampF(v, SCALE_MIN, SCALE_MAX));
            break;
        case FLD_POS_X:
            o->position.x = v;
            dirty = true;
            break;
        case FLD_POS_Y:
            o->position.y = v;
            dirty = true;
            break;
        case FLD_OFF_X:
        case FLD_OFF_Y:
            if (validAnim) {
                Vector2 off = o->animationSet.animations[animSel].animationOffset;
                if (id == FLD_OFF_X)
                    off.x = v;
                else
                    off.y = v;
                AnimationSet_SetAnimationOffset(&o->animationSet, animSel, off);
                dirty = true;
            }
            break;
        case FLD_FPS:
            if (validAnim) {
                /* troca o fps e recalcula o tempo por frame (AnimationSet_SetAnimationData) */
                RayAnimation *anim = &o->animationSet.animations[animSel];
                const int fps = (int)ClampF(roundf(v), 1.0f, 1000.0f);
                AnimationSet_SetAnimationData(&o->animationSet, animSel, fps, anim->looped);
                dirty = true;
            }
            break;
        default:
            break;
    }
}

static void FieldDraw(int id)
{
    const Rectangle r = FieldRect(id);
    const bool active = activeField == id;
    const bool hover = RectHover(r);

    DrawRectangleRec(r, active ? (Color){30, 70, 40, 230} : (Color){18, 18, 18, 220});
    DrawRectangleLinesEx(r, 1, active ? GREEN : (hover ? WHITE : GRAY));

    char text[24];
    if (active && fieldEditing)
        snprintf(text, sizeof(text), "%s_", fieldBuf);
    else if (id == FLD_SCALE)
        snprintf(text, sizeof(text), "%.2f", FieldValue(id));
    else
        snprintf(text, sizeof(text), "%.0f", FieldValue(id));
    CE_Text(text, r.x + 8.0f, r.y + 3.0f, 20.0f, WHITE);
}

/* Retorna true quando um campo esta digitando (ou consumiu Enter/ESC): os atalhos nao rodam. */
static bool FieldsUpdate(void)
{
    if (!hasChara) {
        fieldEditing = false;
        fieldDragging = false;
        activeField = -1;
        return false;
    }

    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const Vector2 m = RayGame_MouseWorld();

    if (fieldEditing) {
        bool consumed = true;
        int c;
        while ((c = GetCharPressed()) > 0) {
            const bool ok = (c >= '0' && c <= '9') || c == '-' || c == '.';
            if (ok && fieldBufLen < (int)sizeof(fieldBuf) - 1) {
                fieldBuf[fieldBufLen++] = (char)c;
                fieldBuf[fieldBufLen] = '\0';
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (fieldBufLen > 0)
                fieldBuf[--fieldBufLen] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (fieldBufLen > 0)
                FieldApply(activeField, strtof(fieldBuf, NULL));
            fieldEditing = false;
            activeField = -1;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            fieldEditing = false;
            activeField = -1;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !RectHover(FieldRect(activeField))) {
            fieldEditing = false;
            activeField = -1;
            consumed = false;
        }
        return consumed && fieldEditing;
    }

    if (fieldDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            const float dx = m.x - fieldDragStartMouse;
            if (fabsf(dx) > 2.0f)
                fieldDragMoved = true;
            float v = fieldDragStartValue + dx * FieldStep(activeField) * (shift ? 10.0f : 1.0f);
            if (activeField != FLD_SCALE)
                v = roundf(v);
            if (v != FieldValue(activeField))
                FieldApply(activeField, v);
            return false;
        }
        fieldDragging = false;
        if (!fieldDragMoved) {
            /* clique sem arrastar: digita um valor novo */
            fieldEditing = true;
            fieldBuf[0] = '\0';
            fieldBufLen = 0;
            return true;
        }
        activeField = -1;
        return false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int id = 0; id < FLD_COUNT; ++id) {
            if (RectHover(FieldRect(id))) {
                activeField = id;
                fieldDragging = true;
                fieldDragMoved = false;
                fieldDragStartMouse = m.x;
                fieldDragStartValue = FieldValue(id);
                break;
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Camera do palco: arrastar com o botao direito ou os botoes de direcao      */
/* ------------------------------------------------------------------------- */

#define PAN_STEP 40.0f

static void PanBy(float dx, float dy)
{
    stagePan.x += dx;
    stagePan.y += dy;
    ApplyStageZoom(stageZoom);
}

static void UpdatePan(void)
{
    const Vector2 m = RayGame_MouseWorld();
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        panning = true;
        panLast = m;
    }
    if (!panning)
        return;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        if (m.x != panLast.x || m.y != panLast.y)
            PanBy(m.x - panLast.x, m.y - panLast.y);
        panLast = m;
    } else {
        panning = false;
    }
}

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

static void Pick_Update(void)
{
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    /* ESC limpa a busca; sem busca, volta ao menu de modding. */
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (filterLen > 0) {
            filter[0] = '\0';
            filterLen = 0;
            FilterChanged();
        } else {
            ModdingMenu_SetScene();
        }
        return;
    }

    if (IsKeyPressed(KEY_F5)) {
        entryCount = CharEd_ScanList(entries, CHARED_LIST_MAX);
        FilterChanged();
        SetStatus(entryCount ? "lista atualizada" : "nenhum personagem encontrado");
    }

    /* Digitacao vai para a busca; Ctrl+V cola a area de transferencia. */
    if (ctrl && IsKeyPressed(KEY_V))
        PasteFilter(GetClipboardText());
    int c;
    while ((c = GetCharPressed()) > 0) {
        if (!ctrl)
            AppendFilterChar(c);
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (filterLen > 0) {
            filter[--filterLen] = '\0';
            FilterChanged();
        }
    }

    if (IsKeyPressed(KEY_DOWN))
        pickSel++;
    if (IsKeyPressed(KEY_UP))
        pickSel--;

    pickWheel += GetMouseWheelMove();
    while (pickWheel >= 1.0f) { pickWheel -= 1.0f; pickSel--; }
    while (pickWheel <= -1.0f) { pickWheel += 1.0f; pickSel++; }
    ClampPickView();

    if (visibleCount > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        OpenEntry(visible[pickSel]);
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 m = RayGame_MouseWorld();
        for (int i = pickTop; i < visibleCount && i < pickTop + PICK_ROWS; ++i) {
            const float y = PICK_Y + (float)(i - pickTop) * PICK_ROW_H;
            if (m.x >= PICK_X && m.x <= PICK_X + PICK_W && m.y >= y && m.y < y + PICK_ROW_H) {
                pickSel = i;
                OpenEntry(visible[i]);
                return;
            }
        }
    }
}

static void Edit_Update(void)
{
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const int count = AnimCount();
    RayAnimationHandler *set = &chara.object.animationSet;

    UpdatePan();
    if (FieldsUpdate()) {
        AnimatedObject_UpdateFrame(&chara.object);
        return;
    }

    if (ctrl && IsKeyPressed(KEY_S)) {
        SaveCurrent();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty && !escArmed) {
            escArmed = true;
            SetStatus("alteracoes nao salvas: Ctrl+S salva, ESC de novo descarta");
        } else {
            CloseCharacter();
        }
        return;
    }

    if (IsKeyPressed(KEY_F)) {
        chara.object.scaleX = -chara.object.scaleX;
        dirty = true;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        const float s = fabsf(chara.object.scaleY) - SCALE_STEP;
        ApplyScale(s < SCALE_MIN ? SCALE_MIN : s);
    }
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        const float s = fabsf(chara.object.scaleY) + SCALE_STEP;
        ApplyScale(s > SCALE_MAX ? SCALE_MAX : s);
    }

    /* Zoom do palco: PageUp/PageDown ou os botoes da tela. */
    if (IsKeyPressed(KEY_PAGE_UP) || RectClicked((Rectangle){ZOOM_IN_X, ZOOM_BTN_Y, 60, 40}))
        ApplyStageZoom(stageZoom * ZOOM_STEP);
    if (IsKeyPressed(KEY_PAGE_DOWN) || RectClicked((Rectangle){ZOOM_OUT_X, ZOOM_BTN_Y, 60, 40}))
        ApplyStageZoom(stageZoom / ZOOM_STEP);
    if (RectClicked((Rectangle){ZOOM_RESET_X, ZOOM_BTN_Y, 100, 40})) {
        stagePan = (Vector2){0.0f, 0.0f};
        ApplyStageZoom(1.0f);
    }

    /* Botoes de direcao: movem o personagem na tela (a camera acompanha). */
    if (RectClicked((Rectangle){PAN_UP_X, PAN_UP_Y, 60, 40}))    PanBy(0.0f, PAN_STEP);
    if (RectClicked((Rectangle){PAN_LEFT_X, PAN_SIDE_Y, 60, 40}))  PanBy(PAN_STEP, 0.0f);
    if (RectClicked((Rectangle){PAN_RIGHT_X, PAN_SIDE_Y, 60, 40})) PanBy(-PAN_STEP, 0.0f);
    if (RectClicked((Rectangle){PAN_UP_X, PAN_DOWN_Y, 60, 40}))  PanBy(0.0f, -PAN_STEP);

    /* Lista de animacoes: W/S, roda do mouse e clique. */
    if (count > 0) {
        animWheel += GetMouseWheelMove();
        int wheelSteps = 0;
        while (animWheel >= 1.0f) { animWheel -= 1.0f; wheelSteps--; }
        while (animWheel <= -1.0f) { animWheel += 1.0f; wheelSteps++; }
        if (wheelSteps != 0)
            SelectAnim(ClampInt(animSel + wheelSteps, 0, count - 1));

        if (IsKeyPressed(KEY_S) && !ctrl)
            SelectAnim((animSel + 1) % count);
        if (IsKeyPressed(KEY_W))
            SelectAnim((animSel + count - 1) % count);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2 m = RayGame_MouseWorld();
            for (int i = animTop; i < count && i < animTop + ANIM_ROWS; ++i) {
                const float y = ANIM_Y + (float)(i - animTop) * ANIM_ROW_H;
                if (m.x >= ANIM_X && m.x <= ANIM_X + ANIM_W && m.y >= y && m.y < y + ANIM_ROW_H) {
                    SelectAnim(i);
                    break;
                }
            }
        }

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
            PlaySelected();
        if (IsKeyPressed(KEY_L) && animSel >= 0 && animSel < count) {
            set->animations[animSel].looped = !set->animations[animSel].looped;
            dirty = true;
        }

        const float step = shift ? 10.0f : 1.0f;
        if (ctrl) {
            /* Ctrl+setas: move o personagem inteiro no palco (position do JSON). */
            float dx = 0.0f, dy = 0.0f;
            if (IsKeyPressed(KEY_LEFT)) dx -= step;
            if (IsKeyPressed(KEY_RIGHT)) dx += step;
            if (IsKeyPressed(KEY_UP)) dy -= step;
            if (IsKeyPressed(KEY_DOWN)) dy += step;
            if (dx != 0.0f || dy != 0.0f) {
                chara.object.position.x += dx;
                chara.object.position.y += dy;
                dirty = true;
            }
        } else {
            /* Setas: offset da animacao selecionada (o que a Psych chama de offsets). */
            if (IsKeyPressed(KEY_LEFT)) NudgeOffset(-step, 0.0f);
            if (IsKeyPressed(KEY_RIGHT)) NudgeOffset(step, 0.0f);
            if (IsKeyPressed(KEY_UP)) NudgeOffset(0.0f, -step);
            if (IsKeyPressed(KEY_DOWN)) NudgeOffset(0.0f, step);
        }
    }

    AnimatedObject_UpdateFrame(&chara.object);
}

static void CharEditor_Update([[maybe_unused]] RayScene *scene)
{
    if (statusTimer > 0.0f)
        statusTimer -= RayGame_DeltaTime();

    if (mode == CE_PICK)
        Pick_Update();
    else
        Edit_Update();
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

/* Render_SetCamera ignora o offset: o target fica no centro do canvas (640,360). Por isso a UI
   usa uiCam (target 640,360) e o palco usa stageCam com target deslocado para a origem cair em
   (STAGE_X, STAGE_Y). */
static void BeginUi(void)
{
    Render_SetCamera(&uiCam);
}

static void EndUi(void)
{
    Render_StopCamera();
}

static void DrawBackground(void)
{
    ClearBackground(BLACK);
    BeginUi();
    Render_DrawGraphicObject(&bg);
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 120});
    EndUi();
}

static void DrawStatus(void)
{
    if (statusTimer > 0.0f && status[0])
        CE_Text(status, 60.0f, 40.0f, 22.0f, YELLOW);
}

static void Pick_Draw(void)
{
    DrawBackground();
    BeginUi();
    CE_Text("Character Editor", PICK_X, 40.0f, 48.0f, WHITE);

    /* caixa de busca */
    DrawRectangle((int)PICK_X, 100, (int)PICK_W, 30, (Color){18, 18, 18, 220});
    DrawRectangleLines((int)PICK_X, 100, (int)PICK_W, 30, filterLen ? GREEN : GRAY);
    if (filterLen > 0) {
        char line[FILTER_MAX + 16];
        snprintf(line, sizeof(line), "busca: %s", filter);
        CE_Text(line, PICK_X + 10.0f, 104.0f, 20.0f, WHITE);
    } else {
        CE_Text("digite para buscar  (Ctrl+V cola)", PICK_X + 10.0f, 104.0f, 20.0f, GRAY);
    }
    char count[24];
    snprintf(count, sizeof(count), "%d/%d", visibleCount, entryCount);
    CE_Text(count, PICK_X + PICK_W - 90.0f, 104.0f, 18.0f, GRAY);

    if (entryCount == 0)
        CE_Text("Nenhum personagem .animset com png encontrado.", PICK_X, PICK_Y, 22.0f, YELLOW);
    else if (visibleCount == 0)
        CE_Text("Nenhum personagem com esse nome.", PICK_X, PICK_Y, 22.0f, YELLOW);

    for (int i = pickTop; i < visibleCount && i < pickTop + PICK_ROWS; ++i) {
        const CharEdEntry *entry = &entries[visible[i]];
        const float y = PICK_Y + (float)(i - pickTop) * PICK_ROW_H;
        const Vector2 m = RayGame_MouseWorld();
        const bool hover = m.x >= PICK_X && m.x <= PICK_X + PICK_W && m.y >= y && m.y < y + PICK_ROW_H;
        const bool selected = i == pickSel;

        Color fill = (Color){18, 18, 18, 200};
        if (selected)
            fill = (Color){42, 120, 60, 220};
        else if (hover)
            fill = (Color){60, 60, 60, 200};
        DrawRectangle((int)PICK_X, (int)y, (int)PICK_W, (int)PICK_ROW_H - 2, fill);

        CE_Text(entry->name, PICK_X + 10.0f, y + 4.0f, 20.0f, WHITE);
        CE_Text(RootLabel(entry->root), PICK_X + 480.0f, y + 6.0f, 18.0f, selected ? WHITE : GRAY);
    }

    CE_Text("setas: escolher   ENTER: abrir   digite: buscar   ESC: limpar busca / voltar   F5: atualizar",
             PICK_X, 660.0f, 18.0f, GRAY);
    DrawStatus();
    EndUi();
}

static void Edit_DrawAnimList(void)
{
    const int count = AnimCount();
    DrawRectangle((int)ANIM_X, (int)(ANIM_Y - 36.0f), (int)ANIM_W,
                  (int)(ANIM_ROWS * ANIM_ROW_H + 46.0f), (Color){0, 0, 0, 170});

    CE_Text(current.name, ANIM_X + 10.0f, ANIM_Y - 32.0f, 22.0f, WHITE);
    if (count == 0) {
        CE_Text("sem animacoes", ANIM_X + 10.0f, ANIM_Y, 18.0f, GRAY);
        return;
    }

    for (int i = animTop; i < count && i < animTop + ANIM_ROWS; ++i) {
        const RayAnimation *anim = &chara.object.animationSet.animations[i];
        const float y = ANIM_Y + (float)(i - animTop) * ANIM_ROW_H;
        const bool selected = i == animSel;

        if (selected)
            DrawRectangle((int)ANIM_X, (int)y, (int)ANIM_W, (int)ANIM_ROW_H - 2, (Color){42, 120, 60, 220});
        CE_Text(anim->name, ANIM_X + 8.0f, y + 2.0f, 17.0f, selected ? WHITE : LIGHTGRAY);
        if (anim->looped)
            CE_Text("loop", ANIM_X + ANIM_W - 46.0f, y + 4.0f, 14.0f, YELLOW);
    }
}

static void Edit_DrawInfo(void)
{
    const float x = INFO_X + 12.0f;
    char line[160];

    DrawRectangle((int)INFO_X, (int)INFO_Y, (int)INFO_W, 290, (Color){0, 0, 0, 170});

    CE_Text("escala", x, 102.0f, 20.0f, WHITE);
    FieldDraw(FLD_SCALE);

    snprintf(line, sizeof(line), "flip: %s", chara.object.scaleX < 0.0f ? "sim" : "nao");
    CE_Text(line, x, 134.0f, 20.0f, WHITE);

    CE_Text("pos", x, 168.0f, 20.0f, WHITE);
    CE_Text("x", 1008.0f, 168.0f, 20.0f, GRAY);
    FieldDraw(FLD_POS_X);
    CE_Text("y", 1124.0f, 168.0f, 20.0f, GRAY);
    FieldDraw(FLD_POS_Y);

    snprintf(line, sizeof(line), "sing_duration: %.1f", chara.singDuration);
    CE_Text(line, x, 200.0f, 20.0f, WHITE);

    if (animSel >= 0 && animSel < AnimCount()) {
        const RayAnimation *anim = &chara.object.animationSet.animations[animSel];
        snprintf(line, sizeof(line), "anim: %s", anim->name);
        CE_Text(line, x, 236.0f, 18.0f, YELLOW);

        CE_Text("off", x, 272.0f, 20.0f, WHITE);
        CE_Text("x", 1008.0f, 272.0f, 20.0f, GRAY);
        FieldDraw(FLD_OFF_X);
        CE_Text("y", 1124.0f, 272.0f, 20.0f, GRAY);
        FieldDraw(FLD_OFF_Y);

        CE_Text("arraste o numero ou clique p/ digitar", x, 302.0f, 14.0f, GRAY);
        CE_Text("fps", x, 322.0f, 20.0f, WHITE);
        FieldDraw(FLD_FPS);
        snprintf(line, sizeof(line), "loop: %s", anim->looped ? "sim" : "nao");
        CE_Text(line, 1140.0f, 322.0f, 20.0f, WHITE);
    }

    CE_Text(dirty ? "* nao salvo" : "salvo", x, INFO_Y + 260.0f, 20.0f, dirty ? YELLOW : GREEN);
}

static void DrawButton(Rectangle r, const char *label)
{
    const bool hover = RectHover(r);
    DrawRectangleRec(r, hover ? (Color){60, 60, 60, 230} : (Color){18, 18, 18, 220});
    DrawRectangleLinesEx(r, 1, hover ? GREEN : GRAY);
    const Vector2 size = MeasureTextEx(mainFont, label, 22.0f, 1.0f);
    CE_Text(label, r.x + (r.width - size.x) * 0.5f, r.y + (r.height - size.y) * 0.5f, 22.0f, WHITE);
}

static void Edit_DrawZoom(void)
{
    char zoomLabel[16];
    snprintf(zoomLabel, sizeof(zoomLabel), "%.0f%%", stageZoom * 100.0f);

    CE_Text("zoom", ZOOM_OUT_X, ZOOM_BTN_Y - 24.0f, 18.0f, GRAY);
    DrawButton((Rectangle){ZOOM_OUT_X, ZOOM_BTN_Y, 60, 40}, "-");
    DrawButton((Rectangle){ZOOM_RESET_X, ZOOM_BTN_Y, 100, 40}, zoomLabel);
    DrawButton((Rectangle){ZOOM_IN_X, ZOOM_BTN_Y, 60, 40}, "+");

    CE_Text("mover", PAN_LEFT_X, PAN_UP_Y - 24.0f, 18.0f, GRAY);
    DrawButton((Rectangle){PAN_UP_X, PAN_UP_Y, 60, 40}, "^");
    DrawButton((Rectangle){PAN_LEFT_X, PAN_SIDE_Y, 60, 40}, "<");
    DrawButton((Rectangle){PAN_RIGHT_X, PAN_SIDE_Y, 60, 40}, ">");
    DrawButton((Rectangle){PAN_UP_X, PAN_DOWN_Y, 60, 40}, "v");
}

static void Edit_DrawHelp(void)
{
    CE_Text("setas: offset (shift = x10)   Ctrl+setas: posicao   W/S: anim   ESPACO: toca   L: loop",
             ANIM_X, 648.0f, 18.0f, GRAY);
    CE_Text("F: flip   -/+: escala   PgUp/PgDn: zoom   Ctrl+S: salvar   ESC: voltar",
             ANIM_X, 672.0f, 18.0f, GRAY);
}

static void Edit_Draw(void)
{
    DrawBackground();

    /* Palco: eixo na origem do personagem e o personagem em si. */
    Render_SetCamera(&stageCam);
    DrawLine(-60, 0, 60, 0, (Color){255, 255, 255, 90});
    DrawLine(0, -60, 0, 60, (Color){255, 255, 255, 90});
    Render_DrawAnimatedObject(&chara.object);
    Render_StopCamera();

    BeginUi();
    Edit_DrawAnimList();
    Edit_DrawInfo();
    Edit_DrawHelp();
    Edit_DrawZoom();
    DrawStatus();
    EndUi();
}

static void CharEditor_Draw([[maybe_unused]] RayScene *scene)
{
    if (mode == CE_PICK)
        Pick_Draw();
    else
        Edit_Draw();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void CharEditor_Create([[maybe_unused]] RayScene *scene)
{
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = MenuBackground_Get();

    /* target = ponto do mundo no centro do canvas; para (0,0) do personagem cair em (STAGE_X, STAGE_Y):
       target = centro - ancora */
    uiCam = (Camera2D){
        .target = {640.0f, 360.0f},
        .offset = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = 1.0f
    };
    stageCam = (Camera2D){
        .target = {640.0f - STAGE_X, 360.0f - STAGE_Y},
        .offset = {0.0f, 0.0f},
        .rotation = 0.0f,
        .zoom = 1.0f
    };

    stagePan = (Vector2){0.0f, 0.0f};
    panning = false;
    activeField = -1;
    fieldDragging = false;
    fieldEditing = false;
    ApplyStageZoom(1.0f);

    mode = CE_PICK;
    entryCount = CharEd_ScanList(entries, CHARED_LIST_MAX);
    filter[0] = '\0';
    filterLen = 0;
    FilterChanged();
    pickWheel = 0.0f;
    hasChara = false;
    current = (CharEdEntry){0};
    animSel = 0;
    animTop = 0;
    animWheel = 0.0f;
    dirty = false;
    escArmed = false;
    status[0] = '\0';
    statusTimer = 0.0f;
    if (entryCount == 0)
        SetStatus("nenhum personagem .animset com png encontrado");
}

static void CharEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    if (hasChara)
        CharEd_FreeCharacter(&chara);
    hasChara = false;
    bg.image = (Texture2D){0}; /* compartilhada: nao descarrega aqui */
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
