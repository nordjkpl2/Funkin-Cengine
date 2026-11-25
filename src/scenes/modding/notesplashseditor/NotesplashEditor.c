/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NotesplashEditor.c - lista de notesplashes: abre qualquer atlas Sparrow de assets/images,
 * escolhe a animacao, cria/remove entradas e define a ativa usada na gameplay.
 * Gravacao: assets/notesplashes.json (ENTER grava; ESC grava se houver alteracao e volta).
 *
 * Controles: W/S campo, A/D troca (Shift = x5 na entrada), clique seleciona/troca,
 * ENTER executa a acao do campo ou grava, ESC volta.
 */

#include "scenes/modding/notesplashseditor/NotesplashEditor.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "scenes/api/FontLoader.h"
#include "data/config/NotesplashConfig.h"
#include "data/config/EffectsConfig.h"
#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define SHEET_DIR "assets/images"
#define SHEETS_MAX 64
#define PREFIXES_MAX 48

#define PREVIEW_X 620.0f
#define PREVIEW_Y 150.0f
#define PREVIEW_W 600.0f
#define PREVIEW_H 470.0f
#define LIST_X 40.0f
#define LIST_Y 130.0f
#define ROW_H 52.0f
#define VALUE_X 330.0f
#define MAX_FIELDS 6
#define SPLASH_PERIOD 0.6f
#define TOAST_TIME 1.6f

typedef enum {
    FLD_ENTRY = 0,
    FLD_SHEET,
    FLD_ANIM,
    FLD_SET_ACTIVE,
    FLD_NEW,
    FLD_REMOVE
} FieldId;

typedef struct {
    FieldId id;
    const char* label;
    int* value;     /* NULL nas acoes */
    int hi;         /* maior indice valido (cursor) */
} Field;

typedef struct {
    char animset[NOTESPLASH_PATH_MAX];
    char prefixes[PREFIXES_MAX][NOTESPLASH_PREFIX_MAX];
    int prefixCount;
} SheetInfo;

static NotesplashList draft;
static SheetInfo sheets[SHEETS_MAX];
static int sheetCount;

static int entry;        /* entrada em edicao */
static int field;        /* campo selecionado */
static int sheetIdx;     /* atlas da entrada atual (indice em sheets) */
static int animIdx;      /* prefixo da entrada atual (indice em sheets[sheetIdx]) */
static char dirty;
static float toastTimer;
static char toastText[96];

static RayAnimationHandler previewSet;
static char previewLoaded[NOTESPLASH_PATH_MAX];  /* atlas carregado no preview */
static int previewAnim = -1;
static RayAnimatedObject previewObj;
static float previewT;
static char previewStale = 1;

/* ------------------------------------------------------------------ */
/* Varredura dos atlas                                                 */
/* ------------------------------------------------------------------ */

static void AddPrefix(SheetInfo* s, const char* pre) {
    for (int i = 0; i < s->prefixCount; i++)
        if (strcmp(s->prefixes[i], pre) == 0) return;
    if (s->prefixCount >= PREFIXES_MAX) return;
    snprintf(s->prefixes[s->prefixCount], NOTESPLASH_PREFIX_MAX, "%s", pre);
    s->prefixCount++;
}

/* Le o XML e junta os prefixos das animacoes (nome do SubTexture sem os digitos finais). */
static char ScanSheet(const char* xmlPath, SheetInfo* out) {
    FILE* f = fopen(xmlPath, "rb");
    if (f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        fclose(f);
        return 0;
    }
    char* buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        return 0;
    }
    const size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    char ok = strstr(buf, "<TextureAtlas") != NULL;
    if (ok) {
        memset(out, 0, sizeof(*out));
        snprintf(out->animset, sizeof(out->animset), "%s", xmlPath);
        const char* p = buf;
        while ((p = strstr(p, "<SubTexture")) != NULL) {
            const char* tagEnd = strchr(p, '>');
            const char* nm = strstr(p, "name=\"");
            if (nm == NULL || (tagEnd != NULL && nm > tagEnd)) {
                p += 11;
                continue;
            }
            nm += 6;
            const char* q = strchr(nm, '"');
            if (q == NULL) break;
            size_t len = (size_t)(q - nm);
            while (len > 0 && nm[len - 1] >= '0' && nm[len - 1] <= '9') len--;
            if (len > 0 && len < NOTESPLASH_PREFIX_MAX) {
                char pre[NOTESPLASH_PREFIX_MAX];
                memcpy(pre, nm, len);
                pre[len] = '\0';
                AddPrefix(out, pre);
            }
            p = q;
        }
    }
    free(buf);
    return ok && out->prefixCount > 0;
}

static int FindSheet(const char* animset) {
    for (int i = 0; i < sheetCount; i++)
        if (strcmp(sheets[i].animset, animset) == 0) return i;
    return -1;
}

/* Atlas que nao esta na pasta (ex.: vindo do json) entra na lista mesmo assim. */
static int EnsureSheet(const char* animset) {
    int i = FindSheet(animset);
    if (i >= 0 || sheetCount >= SHEETS_MAX) return i;
    SheetInfo tmp;
    if (!ScanSheet(animset, &tmp)) return -1;
    sheets[sheetCount] = tmp;
    return sheetCount++;
}

static void ScanAllSheets(void) {
    sheetCount = 0;
    FilePathList files = LoadDirectoryFiles(SHEET_DIR);
    for (unsigned int i = 0; i < files.count && sheetCount < SHEETS_MAX; i++) {
        const char* name = GetFileName(files.paths[i]);
        const size_t len = strlen(name);
        if (len < 5 || strcmp(name + len - 4, ".xml") != 0) continue;

        char path[NOTESPLASH_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", SHEET_DIR, name);
        SheetInfo tmp;
        if (ScanSheet(path, &tmp)) sheets[sheetCount++] = tmp;
    }
    UnloadDirectoryFiles(files);
}

/* ------------------------------------------------------------------ */
/* Estado da entrada atual                                             */
/* ------------------------------------------------------------------ */

static const char* BaseName(const char* path) {
    const char* s = strrchr(path, '/');
    return s ? s + 1 : path;
}

/* Copia do indice da entrada para os cursores sheetIdx/animIdx. */
static void SyncCursors(void) {
    const NotesplashEntry* e = &draft.entries[entry];
    sheetIdx = FindSheet(e->animset);
    if (sheetIdx < 0) sheetIdx = EnsureSheet(e->animset);
    if (sheetIdx < 0) {
        sheetIdx = 0;
        animIdx = 0;
        return;
    }
    animIdx = 0;
    for (int i = 0; i < sheets[sheetIdx].prefixCount; i++) {
        if (strcmp(sheets[sheetIdx].prefixes[i], e->prefix) == 0) {
            animIdx = i;
            break;
        }
    }
}

/* Grava os cursores de volta na entrada (ao trocar atlas ou animacao). */
static void ApplyCursors(void) {
    if (sheetCount == 0) return;
    NotesplashEntry* e = &draft.entries[entry];
    const SheetInfo* s = &sheets[sheetIdx];
    snprintf(e->animset, sizeof(e->animset), "%s", s->animset);
    if (s->prefixCount > 0) snprintf(e->prefix, sizeof(e->prefix), "%s", s->prefixes[animIdx]);
    dirty = 1;
    previewStale = 1;
}

static void Toast(const char* msg) {
    snprintf(toastText, sizeof(toastText), "%s", msg);
    toastTimer = TOAST_TIME;
}

/* ------------------------------------------------------------------ */
/* Campos                                                              */
/* ------------------------------------------------------------------ */

static int BuildFields(Field out[MAX_FIELDS]) {
    int n = 0;
    out[n++] = (Field){FLD_ENTRY, "Entrada", &entry, draft.count - 1};
    out[n++] = (Field){FLD_SHEET, "Arquivo (atlas)", &sheetIdx, sheetCount > 0 ? sheetCount - 1 : 0};
    out[n++] = (Field){FLD_ANIM, "Animacao", &animIdx,
                       sheetCount > 0 && sheets[sheetIdx].prefixCount > 0 ? sheets[sheetIdx].prefixCount - 1 : 0};
    out[n++] = (Field){FLD_SET_ACTIVE, "Usar na gameplay", NULL, 0};
    out[n++] = (Field){FLD_NEW, "Nova entrada (copia)", NULL, 0};
    out[n++] = (Field){FLD_REMOVE, "Remover entrada", NULL, 0};
    return n;
}

static void FormatValue(const Field* f, char* buf, size_t size) {
    switch (f->id) {
        case FLD_ENTRY:
            snprintf(buf, size, "%d / %d", entry + 1, draft.count);
            break;
        case FLD_SHEET:
            if (sheetCount == 0) snprintf(buf, size, "nenhum atlas");
            else snprintf(buf, size, "%s", BaseName(sheets[sheetIdx].animset));
            break;
        case FLD_ANIM:
            if (sheetCount == 0 || sheets[sheetIdx].prefixCount == 0) snprintf(buf, size, "-");
            else snprintf(buf, size, "%s", sheets[sheetIdx].prefixes[animIdx]);
            break;
        case FLD_SET_ACTIVE:
            snprintf(buf, size, "%s", draft.active == entry ? "ativa" : "definir");
            break;
        case FLD_NEW:
        case FLD_REMOVE:
        default:
            snprintf(buf, size, "ENTER");
            break;
    }
}

static void RunAction(FieldId id) {
    switch (id) {
        case FLD_SET_ACTIVE:
            if (draft.active == entry) return;
            draft.active = entry;
            dirty = 1;
            break;
        case FLD_NEW:
            if (draft.count >= NOTESPLASH_MAX) {
                Toast("limite de entradas atingido");
                return;
            }
            draft.entries[draft.count] = draft.entries[entry];
            entry = draft.count;
            draft.count++;
            dirty = 1;
            SyncCursors();
            previewStale = 1;
            break;
        case FLD_REMOVE:
            if (draft.count <= 1) {
                Toast("precisa ter ao menos 1 entrada");
                return;
            }
            for (int i = entry; i < draft.count - 1; i++) draft.entries[i] = draft.entries[i + 1];
            draft.count--;
            if (draft.active == entry) draft.active = 0;
            else if (draft.active > entry) draft.active--;
            if (entry >= draft.count) entry = draft.count - 1;
            dirty = 1;
            SyncCursors();
            previewStale = 1;
            break;
        default:
            break;
    }
}

/* Troca o valor do campo cursor (entrada, atlas ou animacao). */
static void Cycle(const Field* f, int dir) {
    int v = *f->value + dir;
    if (v < 0) v = 0;
    if (v > f->hi) v = f->hi;
    if (v == *f->value) return;
    *f->value = v;

    if (f->id == FLD_ENTRY) {
        SyncCursors();
        previewStale = 1;
    } else if (f->id == FLD_SHEET) {
        animIdx = 0;
        ApplyCursors();
    } else if (f->id == FLD_ANIM) {
        ApplyCursors();
    }
}

static void Activate(const Field* f, int dir) {
    if (f->value != NULL) Cycle(f, dir);
    else RunAction(f->id);
}

static void SaveNow(void) {
    NotesplashConfig_Set(&draft);
    if (NotesplashConfig_Save()) {
        dirty = 0;
        Toast("gravado em assets/notesplashes.json");
    } else {
        Toast("falha ao gravar");
    }
}

/* ------------------------------------------------------------------ */
/* Preview                                                             */
/* ------------------------------------------------------------------ */

static void FreePreview(void) {
    AnimationSet_FreeAll(&previewSet);
    previewLoaded[0] = '\0';
    previewAnim = -1;
}

/* Recarrega o atlas do preview so quando muda; senao so troca a animacao. */
static void RefreshPreview(void) {
    previewStale = 0;
    const NotesplashEntry* e = &draft.entries[entry];
    if (strcmp(previewLoaded, e->animset) != 0) {
        FreePreview();
        char texture[NOTESPLASH_PATH_MAX];
        if (NotesplashConfig_TexturePath(e->animset, texture, sizeof(texture))) {
            previewSet = AnimationSet_LoadAnimations(e->animset, Render_LoadTexture(texture));
            snprintf(previewLoaded, sizeof(previewLoaded), "%s", e->animset);
        }
        Render_DefaultAnimated(&previewObj);
        previewObj.animationSet = previewSet;
    }
    previewAnim = previewLoaded[0] ? AnimationSet_FindAnimation(&previewSet, e->prefix) : -1;
    if (previewAnim >= 0) {
        const int fps = EffectsConfig_Get()->splash.fps;
        AnimationSet_SetAnimationData(&previewSet, previewAnim, fps, 0);
        previewObj.animationSet = previewSet;
        AnimatedObject_SetAnimation(&previewObj, previewAnim);
    }
    previewT = 0.0f;
}

/* Mesmo calculo do splash da gameplay: centro do receptor + scale + offset + tint/alpha do Effects Editor. */
static void DrawPreview(void) {
    const EffectsSplash* cfg = &EffectsConfig_Get()->splash;
    const float cx = PREVIEW_X + PREVIEW_W * 0.5f;
    const float cy = PREVIEW_Y + 200.0f;

    DrawRectangleRounded((Rectangle){cx - 60.0f, cy - 60.0f, 120.0f, 120.0f}, 0.2f, 8, (Color){40, 34, 70, 255});
    DrawRectangleRoundedLinesEx((Rectangle){cx - 60.0f, cy - 60.0f, 120.0f, 120.0f}, 0.2f, 8, 2.0f, (Color){120, 100, 200, 180});

    if (previewAnim < 0) {
        DrawTextEx(mainFont, "animacao nao encontrada", (Vector2){PREVIEW_X + 20.0f, PREVIEW_Y + 340.0f}, 20.0f, 1.0f, (Color){255, 140, 140, 255});
        return;
    }

    const float scale = (float)cfg->scalePct / 100.0f;
    const Vector2 size = AnimatedObject_Sizes(&previewObj);
    const Color tint = EffectsConfig_TintColor(cfg->tint);
    previewObj.scaleX = previewObj.scaleY = scale;
    previewObj.color = (Color){tint.r, tint.g, tint.b, (unsigned char)(cfg->alphaPct * 255 / 100)};
    previewObj.position.x = cx - size.x * scale * 0.5f + (float)cfg->offsetX;
    previewObj.position.y = cy - size.y * scale * 0.5f + (float)cfg->offsetY;
    AnimatedObject_UpdateFrame(&previewObj);
    Render_DrawAnimatedObject(&previewObj);
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void Editor_Update([[maybe_unused]] RayScene* scene) {
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);
    if (toastTimer > 0.0f) toastTimer -= dt;

    previewT += dt;
    if (previewT >= SPLASH_PERIOD) {
        previewT = 0.0f;
        if (previewAnim >= 0) AnimatedObject_SetAnimation(&previewObj, previewAnim);
    }
    if (previewStale) RefreshPreview();

    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);
    if (field >= n) field = n - 1;

    const Vector2 mouse = GetMousePosition();
    const bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty) SaveNow();
        FreePreview();
        ModdingMenu_SetScene();
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (fields[field].value == NULL) RunAction(fields[field].id);
        else SaveNow();
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) field = (field + 1) % n;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) field = (field + n - 1) % n;

    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int dir = 0;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dir = 1;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) dir = -1;
    if (dir != 0 && fields[field].value != NULL) {
        // Shift pula 5 entradas na lista; nos atlas/animacoes anda 1 por vez.
        const int step = (shift && fields[field].id == FLD_ENTRY) ? 5 : 1;
        Field f = fields[field];
        for (int i = 0; i < step; i++) Cycle(&f, dir);
    }

    // Clique numa linha: acao executa; campo troca (metade esquerda volta, direita avanca).
    if (clicked) {
        for (int i = 0; i < n; ++i) {
            const Rectangle row = {LIST_X, LIST_Y + i * ROW_H, PREVIEW_X - LIST_X - 20.0f, ROW_H - 6.0f};
            if (!CheckCollisionPointRec(mouse, row)) continue;
            field = i;
            Activate(&fields[i], mouse.x < VALUE_X + 150.0f ? -1 : 1);
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
        if (sel) DrawRectangleRounded((Rectangle){LIST_X - 12.0f, y - 4.0f, VALUE_X + 300.0f - LIST_X, ROW_H - 8.0f}, 0.3f, 8, (Color){60, 40, 100, 220});

        DrawTextEx(mainFont, fields[i].label, (Vector2){LIST_X, y + 8.0f}, 22.0f, 1.0f, sel ? WHITE : (Color){190, 185, 215, 255});

        char value[80];
        FormatValue(&fields[i], value, sizeof(value));
        char shown[96];
        if (fields[i].value != NULL) snprintf(shown, sizeof(shown), "<  %s  >", value);
        else snprintf(shown, sizeof(shown), "[ %s ]", value);
        DrawTextEx(mainFont, shown, (Vector2){VALUE_X, y + 8.0f}, 20.0f, 1.0f, sel ? (Color){255, 220, 100, 255} : (Color){230, 230, 240, 255});
    }
}

static void Editor_Draw([[maybe_unused]] RayScene* scene) {
    ClearBackground((Color){14, 10, 28, 255});
    DrawRectangleGradientV(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){24, 16, 44, 255}, (Color){8, 6, 18, 255});

    DrawTextEx(mainFont, "NOTESPLASH EDITOR", (Vector2){LIST_X, 26.0f}, 36.0f, 2.0f, WHITE);
    DrawTextEx(mainFont, sheetCount == 0 ? "nenhum atlas Sparrow encontrado em assets/images" : "atlas Sparrow encontrados em assets/images",
               (Vector2){LIST_X, 84.0f}, 18.0f, 1.0f, (Color){190, 185, 220, 255});

    Field fields[MAX_FIELDS];
    const int n = BuildFields(fields);
    DrawFields(fields, n);

    DrawRectangleRounded((Rectangle){PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H}, 0.03f, 8, (Color){8, 6, 18, 230});
    DrawRectangleRoundedLinesEx((Rectangle){PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H}, 0.03f, 8, 1.5f, (Color){120, 100, 200, 170});
    DrawTextEx(mainFont, "PREVIEW", (Vector2){PREVIEW_X + 16.0f, PREVIEW_Y + 12.0f}, 20.0f, 2.0f, (Color){190, 185, 220, 255});
    DrawPreview();

    if (toastTimer > 0.0f)
        DrawTextEx(mainFont, toastText, (Vector2){LIST_X, 640.0f}, 20.0f, 1.0f, (Color){120, 255, 160, 255});
    else if (dirty)
        DrawTextEx(mainFont, "alteracoes nao gravadas", (Vector2){LIST_X, 640.0f}, 20.0f, 1.0f, (Color){255, 200, 90, 255});

    DrawTextEx(mainFont, "W/S campo   A/D troca (shift x5 na entrada)   ENTER acao/grava   ESC volta",
               (Vector2){LIST_X, 680.0f}, 17.0f, 1.0f, (Color){220, 215, 235, 230});
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void Editor_Create([[maybe_unused]] RayScene* scene) {
    draft = *NotesplashConfig_Get();
    entry = draft.active;
    field = 0;
    dirty = 0;
    toastTimer = 0.0f;
    previewT = 0.0f;
    previewStale = 1;
    previewLoaded[0] = '\0';

    ScanAllSheets();
    SyncCursors();
}

static void Editor_Destroy([[maybe_unused]] RayScene* scene) {
    FreePreview();
}

static RayScene scene;
Scene_MakeSceneCode(
    scene,
    NotesplashEditor_SetScene,
    Editor_Create,
    Editor_Update,
    Editor_Draw,
    Editor_Destroy
);
