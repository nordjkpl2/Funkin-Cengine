/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * EffectsEditor.c - editor de efeitos da gameplay: splash das notas, popups de rating e camera base.
 * Tres abas com campos editaveis e um preview ao vivo que usa o mesmo calculo da gameplay.
 * Gravacao: assets/effects.json (ENTER grava; ESC grava se houver alteracao e volta).
 *
 * Controles: W/S campo, A/D ajusta (Shift = x5), TAB troca aba, clique seleciona/ajusta,
 * R restaura a aba, ENTER grava, ESC volta.
 */

#include "scenes/modding/effectseditor/EffectsEditor.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "scenes/api/FontLoader.h"
#include "data/config/EffectsConfig.h"
#include "data/config/NotesplashConfig.h"
#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define UI_ANIMSET "assets/images/ui.animset"
#define UI_TEXTURE "assets/images/ui.png"

#define PREVIEW_X 620.0f
#define PREVIEW_Y 150.0f
#define PREVIEW_W 600.0f
#define PREVIEW_H 470.0f
#define LIST_X 40.0f
#define LIST_Y 160.0f
#define ROW_H 52.0f
#define VALUE_X 330.0f
#define MAX_FIELDS 6
#define SPLASH_PERIOD 0.6f
#define POPUP_PERIOD 0.9f
#define TOAST_TIME 1.6f

typedef enum {
    TAB_SPLASH = 0,
    TAB_POPUP,
    TAB_CAMERA,
    TAB_COUNT
} EditorTab;

typedef enum {
    KIND_INT = 0,
    KIND_TINT,
    KIND_RATING
} FieldKind;

typedef struct {
    const char* label;
    int* value;
    int lo, hi, step;
    FieldKind kind;
} Field;

static const char* const tabNames[TAB_COUNT] = {"SPLASH", "POPUPS", "CAMERA"};
static const char* const ratingNames[EFFECTS_RATINGS] = {"shit", "bad", "good", "sick"};

static EffectsConfig draft;
static int tab;
static int field;
static int popupRating;      /* rating editado na aba Popups (nao vai para o json) */
static char dirty;
static float toastTimer;

static RayAnimationHandler splashSet;
static int splashAnim = -1;
static RayAnimatedObject splashObj;
static float splashT;
static char splashReady;

static RayAnimationHandler uiSet;
static int ratingAnim[EFFECTS_RATINGS];
static RayAnimatedObject popupObj;
static float popupT;
static char uiReady;
static int shownRating = -1;   /* rating que o popup mostra agora */

static Camera2D previewCam;

/* ------------------------------------------------------------------ */
/* Campos por aba                                                      */
/* ------------------------------------------------------------------ */

static int BuildFields(Field out[MAX_FIELDS]) {
    int n = 0;
    switch (tab) {
        case TAB_SPLASH:
            out[n++] = (Field){"Escala %", &draft.splash.scalePct, 10, 300, 5, KIND_INT};
            out[n++] = (Field){"Opacidade %", &draft.splash.alphaPct, 0, 100, 5, KIND_INT};
            out[n++] = (Field){"Velocidade (fps)", &draft.splash.fps, 1, 60, 1, KIND_INT};
            out[n++] = (Field){"Deslocamento X (px)", &draft.splash.offsetX, -1000, 1000, 5, KIND_INT};
            out[n++] = (Field){"Deslocamento Y (px)", &draft.splash.offsetY, -1000, 1000, 5, KIND_INT};
            out[n++] = (Field){"Cor", &draft.splash.tint, 0, EFFECTS_TINTS - 1, 1, KIND_TINT};
            break;
        case TAB_POPUP:
            out[n++] = (Field){"Rating", &popupRating, 0, EFFECTS_RATINGS - 1, 1, KIND_RATING};
            out[n++] = (Field){"Escala %", &draft.popups[popupRating].scalePct, 10, 300, 5, KIND_INT};
            out[n++] = (Field){"Deslocamento X (px)", &draft.popups[popupRating].offsetX, -1000, 1000, 5, KIND_INT};
            out[n++] = (Field){"Deslocamento Y (px)", &draft.popups[popupRating].offsetY, -1000, 1000, 5, KIND_INT};
            break;
        case TAB_CAMERA:
        default:
            out[n++] = (Field){"Zoom % (100 = normal)", &draft.camera.zoomPct, 50, 200, 5, KIND_INT};
            out[n++] = (Field){"Deslocamento X (px)", &draft.camera.offsetX, -1000, 1000, 10, KIND_INT};
            out[n++] = (Field){"Deslocamento Y (px)", &draft.camera.offsetY, -1000, 1000, 10, KIND_INT};
            break;
    }
    return n;
}

static void FormatValue(const Field* f, char* buf, size_t size) {
    if (f->kind == KIND_TINT) snprintf(buf, size, "%s", EffectsConfig_TintName(*f->value));
    else if (f->kind == KIND_RATING) snprintf(buf, size, "%s", ratingNames[*f->value]);
    else snprintf(buf, size, "%d", *f->value);
}

static void Adjust(const Field* f, int dir) {
    int v = *f->value + dir * f->step;
    if (v < f->lo) v = f->lo;
    if (v > f->hi) v = f->hi;
    if (v == *f->value) return;
    *f->value = v;
    if (f->kind != KIND_RATING) dirty = 1;   /* trocar o rating nao altera o arquivo */
}

static void ResetTab(void) {
    EffectsConfig def;
    EffectsConfig_Defaults(&def);
    switch (tab) {
        case TAB_SPLASH: draft.splash = def.splash; break;
        case TAB_POPUP:  draft.popups[popupRating] = def.popups[popupRating]; break;
        case TAB_CAMERA: draft.camera = def.camera; break;
        default: break;
    }
    dirty = 1;
}

static void SaveNow(void) {
    EffectsConfig_Set(&draft);
    if (EffectsConfig_Save()) {
        dirty = 0;
        toastTimer = TOAST_TIME;
    }
}

/* ------------------------------------------------------------------ */
/* Preview                                                             */
/* ------------------------------------------------------------------ */

/* Splash na mesma conta da gameplay: centro do receptor + scale + offset + tint/alpha. */
static void DrawSplashPreview(void) {
    const EffectsSplash* cfg = &draft.splash;
    const float cx = PREVIEW_X + PREVIEW_W * 0.5f;
    const float cy = PREVIEW_Y + 200.0f;

    // Receptor fake (quadrado) no centro.
    DrawRectangleRounded((Rectangle){cx - 60.0f, cy - 60.0f, 120.0f, 120.0f}, 0.2f, 8, (Color){40, 34, 70, 255});
    DrawRectangleRoundedLinesEx((Rectangle){cx - 60.0f, cy - 60.0f, 120.0f, 120.0f}, 0.2f, 8, 2.0f, (Color){120, 100, 200, 180});

    if (!splashReady || splashAnim < 0) return;

    AnimationSet_SetAnimationData(&splashSet, splashAnim, cfg->fps, 0);
    const float scale = (float)cfg->scalePct / 100.0f;
    const Vector2 size = AnimatedObject_Sizes(&splashObj);
    const Color tint = EffectsConfig_TintColor(cfg->tint);

    splashObj.scaleX = splashObj.scaleY = scale;
    splashObj.color = (Color){tint.r, tint.g, tint.b, (unsigned char)(cfg->alphaPct * 255 / 100)};
    splashObj.position.x = cx - size.x * scale * 0.5f + (float)cfg->offsetX;
    splashObj.position.y = cy - size.y * scale * 0.5f + (float)cfg->offsetY;
    AnimatedObject_UpdateFrame(&splashObj);
    Render_DrawAnimatedObject(&splashObj);
}

static void DrawPopupPreview(void) {
    if (!uiReady) return;
    const EffectsPopup* cfg = &draft.popups[popupRating];
    const float scale = (float)cfg->scalePct / 100.0f;
    const float cx = PREVIEW_X + PREVIEW_W * 0.5f + (float)cfg->offsetX;
    const float baseY = PREVIEW_Y + 150.0f + (float)cfg->offsetY;

    popupObj.scaleX = popupObj.scaleY = scale;
    const Vector2 size = AnimatedObject_Sizes(&popupObj);
    popupObj.position.x = cx - size.x * 0.5f;
    popupObj.position.y = baseY;
    popupObj.color.a = 255;
    AnimatedObject_UpdateFrame(&popupObj);
    Render_DrawAnimatedObject(&popupObj);
}

/* Camera: mundo de referencia 1280x720 visto com o zoom e o deslocamento base da config. */
static void DrawCameraPreview(void) {
    const EffectsCamera* cfg = &draft.camera;
    const float view = 0.47f;   /* 600 px de preview ~ 1280 px de jogo */
    previewCam = (Camera2D){0};
    previewCam.offset = (Vector2){PREVIEW_X + PREVIEW_W * 0.5f, PREVIEW_Y + PREVIEW_H * 0.5f};
    previewCam.target = (Vector2){640.0f + (float)cfg->offsetX, 360.0f + (float)cfg->offsetY};
    previewCam.zoom = view * (float)cfg->zoomPct / 100.0f;

    BeginScissorMode((int)PREVIEW_X, (int)PREVIEW_Y, (int)PREVIEW_W, (int)PREVIEW_H);
    BeginMode2D(previewCam);
    DrawRectangle(0, 0, 1280, 720, (Color){30, 24, 52, 255});
    DrawRectangle(0, 470, 1280, 250, (Color){60, 40, 90, 255});   /* chao */
    DrawRectangle(360, 230, 200, 240, (Color){255, 110, 150, 255}); /* oponente */
    DrawRectangle(820, 230, 200, 240, (Color){110, 200, 255, 255}); /* BF */
    DrawRectangleLines(0, 0, 1280, 720, (Color){255, 255, 255, 90});  /* limite do jogo */
    DrawLine(640, 0, 640, 720, (Color){255, 255, 255, 40});
    EndMode2D();
    EndScissorMode();
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void SwitchTab(int next) {
    tab = (next + TAB_COUNT) % TAB_COUNT;
    field = 0;
}

static void Editor_Update([[maybe_unused]] RayScene* scene) {
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);
    if (toastTimer > 0.0f) toastTimer -= dt;

    splashT += dt;
    if (splashT >= SPLASH_PERIOD) {
        splashT = 0.0f;
        if (splashReady) AnimatedObject_SetAnimation(&splashObj, splashAnim);
    }
    popupT += dt;
    if (popupT >= POPUP_PERIOD) {
        popupT = 0.0f;
        if (uiReady) AnimatedObject_SetAnimation(&popupObj, ratingAnim[popupRating]);
    }

    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);
    if (field >= n) field = n - 1;

    // Abas: TAB, ou clique na aba.
    if (IsKeyPressed(KEY_TAB)) SwitchTab(tab + 1);
    const Vector2 mouse = GetMousePosition();
    const bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    for (int i = 0; i < TAB_COUNT && clicked; ++i) {
        if (CheckCollisionPointRec(mouse, (Rectangle){LIST_X + i * 170.0f, 86.0f, 160.0f, 40.0f})) {
            SwitchTab(i);
            return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty) SaveNow();
        ModdingMenu_SetScene();
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) SaveNow();
    if (IsKeyPressed(KEY_R)) ResetTab();

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) field = (field + 1) % n;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) field = (field + n - 1) % n;

    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const int mult = shift ? 5 : 1;
    int dir = 0;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressedRepeat(KEY_D)) dir = 1;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressedRepeat(KEY_A)) dir = -1;
    if (dir != 0) {
        Field f = fields[field];
        f.step *= mult;
        Adjust(&f, dir);
    }
    if (tab == TAB_POPUP && uiReady && shownRating != popupRating) {
        shownRating = popupRating;
        AnimatedObject_SetAnimation(&popupObj, ratingAnim[popupRating]);
    }

    // Clique numa linha: seleciona; metade esquerda diminui, direita aumenta.
    if (clicked) {
        for (int i = 0; i < n; ++i) {
            const Rectangle row = {LIST_X, LIST_Y + i * ROW_H, PREVIEW_X - LIST_X - 20.0f, ROW_H - 6.0f};
            if (!CheckCollisionPointRec(mouse, row)) continue;
            field = i;
            Field f = fields[i];
            f.step *= mult;
            Adjust(&f, mouse.x < VALUE_X + 150.0f ? -1 : 1);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void DrawFields(const Field* fields, int n) {
    for (int i = 0; i < n; ++i) {
        const float y = LIST_Y + i * ROW_H;
        const bool sel = i == field;
        if (sel) DrawRectangleRounded((Rectangle){LIST_X - 12.0f, y - 4.0f, VALUE_X + 220.0f - LIST_X, ROW_H - 8.0f}, 0.3f, 8, (Color){60, 40, 100, 220});

        DrawTextEx(mainFont, fields[i].label, (Vector2){LIST_X, y + 8.0f}, 22.0f, 1.0f, sel ? WHITE : (Color){190, 185, 215, 255});

        char value[32];
        FormatValue(&fields[i], value, sizeof(value));
        char shown[48];
        snprintf(shown, sizeof(shown), "<  %s  >", value);
        DrawTextEx(mainFont, shown, (Vector2){VALUE_X, y + 8.0f}, 22.0f, 1.0f, sel ? (Color){255, 220, 100, 255} : (Color){230, 230, 240, 255});
    }
}

static void Editor_Draw([[maybe_unused]] RayScene* scene) {
    ClearBackground((Color){14, 10, 28, 255});
    DrawRectangleGradientV(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){24, 16, 44, 255}, (Color){8, 6, 18, 255});

    DrawTextEx(mainFont, "EFFECTS EDITOR", (Vector2){LIST_X, 26.0f}, 36.0f, 2.0f, WHITE);

    for (int i = 0; i < TAB_COUNT; ++i) {
        const Rectangle r = {LIST_X + i * 170.0f, 86.0f, 160.0f, 40.0f};
        const bool on = i == tab;
        DrawRectangleRounded(r, 0.3f, 8, on ? (Color){255, 92, 176, 255} : (Color){40, 32, 64, 255});
        const Vector2 tsz = MeasureTextEx(mainFont, tabNames[i], 20.0f, 1.0f);
        DrawTextEx(mainFont, tabNames[i], (Vector2){r.x + (r.width - tsz.x) * 0.5f, r.y + 9.0f}, 20.0f, 1.0f, WHITE);
    }

    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);
    DrawFields(fields, n);

    // Painel de preview.
    DrawRectangleRounded((Rectangle){PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H}, 0.03f, 8, (Color){8, 6, 18, 230});
    DrawRectangleRoundedLinesEx((Rectangle){PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H}, 0.03f, 8, 1.5f, (Color){120, 100, 200, 170});
    DrawTextEx(mainFont, "PREVIEW", (Vector2){PREVIEW_X + 16.0f, PREVIEW_Y + 12.0f}, 20.0f, 2.0f, (Color){190, 185, 220, 255});

    if (tab == TAB_SPLASH) DrawSplashPreview();
    else if (tab == TAB_POPUP) DrawPopupPreview();
    else DrawCameraPreview();

    // Estado e ajuda.
    if (toastTimer > 0.0f)
        DrawTextEx(mainFont, "gravado em assets/effects.json", (Vector2){LIST_X, 640.0f}, 20.0f, 1.0f, (Color){120, 255, 160, 255});
    else if (dirty)
        DrawTextEx(mainFont, "alteracoes nao gravadas", (Vector2){LIST_X, 640.0f}, 20.0f, 1.0f, (Color){255, 200, 90, 255});

    DrawTextEx(mainFont, "W/S campo   A/D ajusta (shift x5)   TAB aba   R restaura   ENTER grava   ESC volta",
               (Vector2){LIST_X, 680.0f}, 17.0f, 1.0f, (Color){220, 215, 235, 230});
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void Editor_Create([[maybe_unused]] RayScene* scene) {
    draft = *EffectsConfig_Get();
    tab = TAB_SPLASH;
    field = 0;
    popupRating = EFFECTS_RATINGS - 1;        /* sick: o mais comum na tela */
    dirty = 0;
    toastTimer = 0.0f;
    splashT = 0.0f;
    popupT = 0.0f;

    // Splash = entrada ativa do Notesplash Editor (assets/notesplashes.json).
    const NotesplashEntry* entry = NotesplashConfig_Active();
    char texture[NOTESPLASH_PATH_MAX];
    if (NotesplashConfig_TexturePath(entry->animset, texture, sizeof(texture))) {
        splashSet = AnimationSet_LoadAnimations(entry->animset, Render_LoadTexture(texture));
        AnimationSet_SetData(splashAnim, splashSet, entry->prefix, draft.splash.fps, 0, 0, 0);
    }
    splashReady = splashAnim >= 0;
    if (splashReady) {
        Render_DefaultAnimated(&splashObj);
        splashObj.animationSet = splashSet;
        AnimatedObject_SetAnimation(&splashObj, splashAnim);
    }

    uiSet = AnimationSet_LoadAnimations(UI_ANIMSET, Render_LoadTexture(UI_TEXTURE));
    ratingAnim[0] = AnimationSet_FindAnimation(&uiSet, "shit");
    ratingAnim[1] = AnimationSet_FindAnimation(&uiSet, "bad");
    ratingAnim[2] = AnimationSet_FindAnimation(&uiSet, "good");
    ratingAnim[3] = AnimationSet_FindAnimation(&uiSet, "sick");
    uiReady = 1;
    Render_DefaultAnimated(&popupObj);
    popupObj.animationSet = uiSet;
    AnimatedObject_SetAnimation(&popupObj, ratingAnim[popupRating]);
    shownRating = popupRating;
}

static void Editor_Destroy([[maybe_unused]] RayScene* scene) {
    AnimationSet_FreeAll(&splashSet);
    AnimationSet_FreeAll(&uiSet);
    splashReady = 0;
    uiReady = 0;
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
