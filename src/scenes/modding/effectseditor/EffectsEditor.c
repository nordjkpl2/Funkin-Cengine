/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * EffectsEditor.c - editor de efeitos da gameplay: splash das notas, popups de rating e camera base.
 * Tres abas com campos numericos (arrastar, roda do mouse ou clicar e digitar) e um preview ao vivo
 * com a mesma conta da gameplay. Gravacao: assets/effects.json (Ctrl+S).
 *
 * Controles: Tab/1-3 troca aba, Ctrl+S salva, R restaura a aba, Esc volta (pergunta se ha mudancas).
 */

#include "scenes/modding/effectseditor/EffectsEditor.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "data/config/EffectsConfig.h"
#include "data/config/NotesplashConfig.h"
#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define UI_ANIMSET "assets/images/ui.animset"
#define UI_TEXTURE "assets/images/ui.png"

#define SPLASH_PERIOD 0.6f
#define POPUP_PERIOD  0.9f
#define MAX_FIELDS    5
#define ROW_H         58.0f

enum { CONFIRM_LEAVE = 1 };

typedef enum {
    TAB_SPLASH = 0,
    TAB_POPUP,
    TAB_CAMERA,
    TAB_COUNT
} EditorTab;

typedef struct {
    const char *label;
    const char *unit;
    int *value;
    int lo, hi;
} Field;

static const char *const tabNames[TAB_COUNT] = {"Splash", "Popups", "Camera"};
static const char *const ratingNames[EFFECTS_RATINGS] = {"shit", "bad", "good", "sick"};

static const Rectangle kLeft = {16.0f, 62.0f, 560.0f, 620.0f};
static const Rectangle kRight = {588.0f, 62.0f, 676.0f, 620.0f};
static const Rectangle kTabs = {32.0f, 78.0f, 528.0f, 38.0f};
static const Rectangle kBtnSave = {1134.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnReset = {954.0f, 8.0f, 170.0f, 32.0f};

static EffectsConfig draft;
static int tab;
static int popupRating;      /* rating mostrado/editado na aba Popups (nao vai para o json) */
static bool dirty;

static RayAnimationHandler splashSet;
static int splashAnim = -1;
static RayAnimatedObject splashObj;
static float splashT;

static RayAnimationHandler uiSet;
static int ratingAnim[EFFECTS_RATINGS];
static RayAnimatedObject popupObj;
static float popupT;
static int shownRating = -1;

/* ------------------------------------------------------------------ */
/* Campos                                                              */
/* ------------------------------------------------------------------ */

static int BuildFields(Field out[MAX_FIELDS])
{
    int n = 0;
    switch (tab) {
        case TAB_SPLASH:
            out[n++] = (Field){"Escala", "%", &draft.splash.scalePct, 10, 300};
            out[n++] = (Field){"Opacidade", "%", &draft.splash.alphaPct, 0, 100};
            out[n++] = (Field){"Velocidade", "fps", &draft.splash.fps, 1, 60};
            out[n++] = (Field){"Deslocamento X", "px", &draft.splash.offsetX, -1000, 1000};
            out[n++] = (Field){"Deslocamento Y", "px", &draft.splash.offsetY, -1000, 1000};
            break;
        case TAB_POPUP:
            out[n++] = (Field){"Escala", "%", &draft.popups[popupRating].scalePct, 10, 300};
            out[n++] = (Field){"Deslocamento X", "px", &draft.popups[popupRating].offsetX, -1000, 1000};
            out[n++] = (Field){"Deslocamento Y", "px", &draft.popups[popupRating].offsetY, -1000, 1000};
            break;
        case TAB_CAMERA:
        default:
            out[n++] = (Field){"Zoom (100 = normal)", "%", &draft.camera.zoomPct, 50, 200};
            out[n++] = (Field){"Deslocamento X", "px", &draft.camera.offsetX, -1000, 1000};
            out[n++] = (Field){"Deslocamento Y", "px", &draft.camera.offsetY, -1000, 1000};
            break;
    }
    return n;
}

/* Linha 0 da aba Popups e o seletor de rating; os campos comecam abaixo dele. */
static float FieldsTop(void)
{
    return kLeft.y + (tab == TAB_POPUP ? 196.0f : 120.0f);
}

static Rectangle FieldRect(int i)
{
    return (Rectangle){kLeft.x + 300.0f, FieldsTop() + (float)i * ROW_H, 170.0f, 36.0f};
}

static Rectangle RatingRect(int i)
{
    return (Rectangle){kLeft.x + 20.0f + (float)i * 130.0f, kLeft.y + 136.0f, 120.0f, 36.0f};
}

static Rectangle TintRect(int i)
{
    return (Rectangle){kLeft.x + 20.0f + (float)i * 76.0f, FieldsTop() + 5.0f * ROW_H + 34.0f, 64.0f, 40.0f};
}

static void ResetTab(void)
{
    EffectsConfig def;
    EffectsConfig_Defaults(&def);
    switch (tab) {
        case TAB_SPLASH: draft.splash = def.splash; break;
        case TAB_POPUP:  draft.popups[popupRating] = def.popups[popupRating]; break;
        default:         draft.camera = def.camera; break;
    }
    dirty = true;
    MUi_SetStatus(MUI_INFO, "aba %s restaurada ao padrao (Ctrl+S grava)", tabNames[tab]);
}

static bool SaveNow(void)
{
    EffectsConfig_Set(&draft);
    if (!EffectsConfig_Save()) {
        MUi_SetStatus(MUI_ERR, "falha ao gravar assets/effects.json");
        return false;
    }
    dirty = false;
    MUi_SetStatus(MUI_OK, "gravado em assets/effects.json");
    return true;
}

/* ------------------------------------------------------------------ */
/* Preview                                                             */
/* ------------------------------------------------------------------ */

static Rectangle PreviewRect(void)
{
    return (Rectangle){kRight.x + 12.0f, kRight.y + 40.0f, kRight.width - 24.0f, kRight.height - 52.0f};
}

static void UpdatePreview(float dt)
{
    const Rectangle area = PreviewRect();
    const float cx = area.x + area.width * 0.5f;

    splashT += dt;
    if (splashT >= SPLASH_PERIOD) {
        splashT = 0.0f;
        if (splashAnim >= 0)
            AnimatedObject_SetAnimation(&splashObj, splashAnim);
    }
    popupT += dt;
    if (popupT >= POPUP_PERIOD || shownRating != popupRating) {
        popupT = 0.0f;
        shownRating = popupRating;
        if (ratingAnim[popupRating] >= 0)
            AnimatedObject_SetAnimation(&popupObj, ratingAnim[popupRating]);
    }

    if (tab == TAB_SPLASH && splashAnim >= 0) {
        const EffectsSplash *cfg = &draft.splash;
        const float cy = area.y + area.height * 0.45f;
        const float scale = (float)cfg->scalePct / 100.0f;
        const Color tint = EffectsConfig_TintColor(cfg->tint);
        AnimationSet_SetAnimationData(&splashSet, splashAnim, cfg->fps, 0);
        splashObj.animationSet = splashSet;
        splashObj.scaleX = splashObj.scaleY = scale;
        splashObj.color = (Color){tint.r, tint.g, tint.b, (unsigned char)(cfg->alphaPct * 255 / 100)};
        AnimatedObject_UpdateFrame(&splashObj);
        const Vector2 size = AnimatedObject_Sizes(&splashObj);
        splashObj.position.x = cx - size.x * scale * 0.5f + (float)cfg->offsetX;
        splashObj.position.y = cy - size.y * scale * 0.5f + (float)cfg->offsetY;
    } else if (tab == TAB_POPUP && ratingAnim[popupRating] >= 0) {
        const EffectsPopup *cfg = &draft.popups[popupRating];
        const float scale = (float)cfg->scalePct / 100.0f;
        popupObj.scaleX = popupObj.scaleY = scale;
        popupObj.color.a = 255;
        AnimatedObject_UpdateFrame(&popupObj);
        const Vector2 size = AnimatedObject_Sizes(&popupObj);
        popupObj.position.x = cx + (float)cfg->offsetX - size.x * 0.5f;
        popupObj.position.y = area.y + 170.0f + (float)cfg->offsetY;
    }
}

static void DrawSplashPreview(Rectangle area)
{
    const float cx = area.x + area.width * 0.5f;
    const float cy = area.y + area.height * 0.45f;
    const Rectangle receptor = {cx - 56.0f, cy - 56.0f, 112.0f, 112.0f};
    DrawRectangleRounded(receptor, 0.25f, 8, (Color){40, 34, 70, 255});
    DrawRectangleRoundedLinesEx(receptor, 0.25f, 8, 2.0f, (Color){120, 100, 200, 180});
    if (splashAnim >= 0)
        Render_DrawAnimatedObject(&splashObj);
    else
        MUi_TextCenter("splash ativo sem animacao (Notesplash Editor)", (Rectangle){area.x, cy + 80.0f, area.width, 30.0f}, 18.0f, MUI_C_BAD);
}

static void DrawPopupPreview(Rectangle area)
{
    if (ratingAnim[popupRating] >= 0)
        Render_DrawAnimatedObject(&popupObj);
    else
        MUi_TextCenter("ui.animset sem essa animacao", (Rectangle){area.x, area.y + 200.0f, area.width, 30.0f}, 18.0f, MUI_C_BAD);
}

/* Camera: mundo de referencia 1280x720 visto com o zoom e o deslocamento base da config. */
static void DrawCameraPreview(Rectangle area)
{
    const EffectsCamera *cfg = &draft.camera;
    const float view = fminf(area.width / 1280.0f, area.height / 720.0f);
    Camera2D cam = {0};
    cam.offset = (Vector2){area.x + area.width * 0.5f, area.y + area.height * 0.5f};
    cam.target = (Vector2){640.0f + (float)cfg->offsetX, 360.0f + (float)cfg->offsetY};
    cam.zoom = view * (float)cfg->zoomPct / 100.0f;

    BeginMode2D(cam);
    DrawRectangle(-640, -360, 2560, 1440, (Color){18, 14, 34, 255});
    DrawRectangle(0, 0, 1280, 720, (Color){30, 24, 52, 255});
    DrawRectangle(0, 470, 1280, 250, (Color){60, 40, 90, 255});      /* chao */
    DrawRectangle(360, 230, 200, 240, (Color){255, 110, 150, 255});  /* oponente */
    DrawRectangle(820, 230, 200, 240, (Color){110, 200, 255, 255});  /* BF */
    DrawRectangleLinesEx((Rectangle){0, 0, 1280, 720}, 3.0f, (Color){255, 255, 255, 90});
    DrawLine(640, 0, 640, 720, (Color){255, 255, 255, 40});
    EndMode2D();

    /* moldura da tela do jogo */
    const float sw = 1280.0f * view, sh = 720.0f * view;
    DrawRectangleLinesEx((Rectangle){area.x + (area.width - sw) * 0.5f, area.y + (area.height - sh) * 0.5f, sw, sh},
                         2.0f, MUI_C_ACCENT);
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void SwitchTab(int next)
{
    tab = (next + TAB_COUNT) % TAB_COUNT;
    MUi_TextBlur();
}

static void Editor_Update([[maybe_unused]] RayScene *scene)
{
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);
    MUi_FrameBegin();
    MUi_Tick(dt);
    UpdatePreview(dt);

    if (MUi_ModalActive()) {
        int id = 0;
        const MUiModalResult res = MUi_ModalUpdate(&id);
        if (id == CONFIRM_LEAVE && ((res == MUI_MODAL_OK && SaveNow()) || res == MUI_MODAL_DISCARD))
            ModdingMenu_SetScene();
        return;
    }

    if (MUi_Button(kBtnSave))
        SaveNow();
    if (MUi_Button(kBtnReset))
        ResetTab();

    const int clickedTab = MUi_Tabs(kTabs, TAB_COUNT, tab);
    if (clickedTab != tab)
        SwitchTab(clickedTab);

    if (tab == TAB_POPUP) {
        for (int i = 0; i < EFFECTS_RATINGS; ++i) {
            if (MUi_Button(RatingRect(i)))
                popupRating = i;
        }
    }

    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);
    for (int i = 0; i < n; ++i) {
        if (MUi_NumInt(FieldRect(i), fields[i].value, 1, fields[i].lo, fields[i].hi))
            dirty = true;
    }
    if (tab == TAB_SPLASH) {
        for (int i = 0; i < EFFECTS_TINTS; ++i) {
            if (MUi_Button(TintRect(i)) && draft.splash.tint != i) {
                draft.splash.tint = i;
                dirty = true;
            }
        }
    }

    if (!MUi_KeysFree())
        return;

    if (MUi_Ctrl() && IsKeyPressed(KEY_S)) {
        SaveNow();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_LEAVE, "Sair do Effects Editor", "Ha alteracoes nao gravadas.");
        else
            ModdingMenu_SetScene();
        return;
    }
    if (IsKeyPressed(KEY_TAB))
        SwitchTab(tab + (MUi_Shift() ? -1 : 1));
    for (int i = 0; i < TAB_COUNT; ++i) {
        if (IsKeyPressed(KEY_ONE + i))
            SwitchTab(i);
    }
    if (IsKeyPressed(KEY_R))
        ResetTab();
    if (tab == TAB_POPUP) {
        if (MUi_Rep(KEY_LEFT))
            popupRating = (popupRating + EFFECTS_RATINGS - 1) % EFFECTS_RATINGS;
        if (MUi_Rep(KEY_RIGHT))
            popupRating = (popupRating + 1) % EFFECTS_RATINGS;
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void DrawFields(void)
{
    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);

    if (tab == TAB_POPUP) {
        MUi_Text("Rating", kLeft.x + 20.0f, kLeft.y + 112.0f, 17.0f, MUI_C_DIM);
        for (int i = 0; i < EFFECTS_RATINGS; ++i)
            MUi_DrawButton(RatingRect(i), ratingNames[i], i == popupRating ? MUI_BTN_ACTIVE : MUI_BTN_NORMAL);
    }

    for (int i = 0; i < n; ++i) {
        const Rectangle r = FieldRect(i);
        MUi_Text(fields[i].label, kLeft.x + 24.0f, r.y + 8.0f, 20.0f, MUI_C_TEXT);
        MUi_DrawNum(r, (float)*fields[i].value, 1.0f);
        MUi_Text(fields[i].unit, r.x + r.width + 10.0f, r.y + 9.0f, 17.0f, MUI_C_FAINT);
    }

    if (tab == TAB_SPLASH) {
        MUi_Text("Cor", kLeft.x + 24.0f, FieldsTop() + 5.0f * ROW_H + 6.0f, 20.0f, MUI_C_TEXT);
        for (int i = 0; i < EFFECTS_TINTS; ++i)
            MUi_DrawSwatch(TintRect(i), EffectsConfig_TintColor(i), draft.splash.tint == i);
        MUi_Text(EffectsConfig_TintName(draft.splash.tint), kLeft.x + 24.0f, FieldsTop() + 5.0f * ROW_H + 86.0f, 17.0f, MUI_C_DIM);
        MUi_Text("A animacao/atlas do splash vem do Notesplash Editor.", kLeft.x + 24.0f, kLeft.y + kLeft.height - 34.0f, 16.0f, MUI_C_FAINT);
    } else if (tab == TAB_CAMERA) {
        MUi_Text("Zoom e deslocamento base da camera de jogo.", kLeft.x + 24.0f, kLeft.y + kLeft.height - 34.0f, 16.0f, MUI_C_FAINT);
    }
    MUi_Text("arraste o numero, use a roda ou clique e digite (Shift x10)", kLeft.x + 24.0f, kLeft.y + kLeft.height - 58.0f, 16.0f, MUI_C_FAINT);
}

static void Editor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    MUi_DrawTopBar("Effects Editor", "assets/effects.json", dirty);
    MUi_DrawButton(kBtnReset, "Restaurar aba (R)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnSave, "Salvar", dirty ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);

    MUi_DrawPanel(kLeft);
    MUi_DrawTabs(kTabs, tabNames, TAB_COUNT, tab);
    DrawFields();

    MUi_DrawPanel(kRight);
    MUi_DrawSection(kRight.x + 14.0f, kRight.y + 12.0f, "Preview");
    const Rectangle area = PreviewRect();
    DrawRectangleRounded(area, 0.02f, 6, (Color){6, 4, 14, 255});
    MUi_ClipBegin(area);
    if (tab == TAB_SPLASH)
        DrawSplashPreview(area);
    else if (tab == TAB_POPUP)
        DrawPopupPreview(area);
    else
        DrawCameraPreview(area);
    MUi_ClipEnd();

    MUi_DrawBottomBar("Tab/1-3 aba   Ctrl+S salva   R restaura a aba   setas: rating (Popups)   Esc volta");
    MUi_DrawModal();
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void Editor_Create([[maybe_unused]] RayScene *scene)
{
    MUi_Reset();
    draft = *EffectsConfig_Get();
    tab = TAB_SPLASH;
    popupRating = EFFECTS_RATINGS - 1;   /* sick: o mais comum na tela */
    dirty = false;
    splashT = popupT = 0.0f;
    shownRating = -1;

    /* Splash = entrada ativa do Notesplash Editor (assets/notesplashes.json). */
    splashSet = (RayAnimationHandler){0};
    splashAnim = -1;
    const NotesplashEntry *entry = NotesplashConfig_Active();
    char texture[NOTESPLASH_PATH_MAX];
    if (NotesplashConfig_TexturePath(entry->animset, texture, sizeof(texture)) && FileExists(texture)) {
        splashSet = AnimationSet_LoadAnimations(entry->animset, Render_LoadTexture(texture));
        AnimationSet_SetData(splashAnim, splashSet, entry->prefix, draft.splash.fps, 0, 0, 0);
    }
    Render_DefaultAnimated(&splashObj);
    splashObj.animationSet = splashSet;
    if (splashAnim >= 0)
        AnimatedObject_SetAnimation(&splashObj, splashAnim);

    uiSet = AnimationSet_LoadAnimations(UI_ANIMSET, Render_LoadTexture(UI_TEXTURE));
    for (int i = 0; i < EFFECTS_RATINGS; ++i)
        ratingAnim[i] = AnimationSet_FindAnimation(&uiSet, ratingNames[i]);
    Render_DefaultAnimated(&popupObj);
    popupObj.animationSet = uiSet;
}

static void Editor_Destroy([[maybe_unused]] RayScene *scene)
{
    AnimationSet_FreeAll(&splashSet);
    AnimationSet_FreeAll(&uiSet);
    splashAnim = -1;
    MUi_Reset();
}

static RayScene scene;
Scene_MakeSceneCode(
    scene,
    EffectsEditor_SetScene,
    Editor_Create,
    Editor_Update,
    Editor_Draw,
    Editor_Destroy
);
