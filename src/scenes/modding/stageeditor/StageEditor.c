/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Stage Editor: escolhe um stage (lista com busca ou nome novo) e monta o palco num canvas com
 * as imagens reais (animadas tocam), os personagens (bf, gf, dad) nas posicoes do .json e a
 * moldura da tela do jogo no zoom padrao. Grava <raiz>/stages/<nome>.lua (sprites) e .json.
 *
 * Canvas: esq. seleciona/arrasta (sprite ou personagem) | dir./meio arrasta a vista |
 *         roda = zoom na vista | Shift+roda = escala do sprite
 * Teclas: setas movem (Shift x10) | PgUp/PgDn troca selecao | Ctrl+Up/Down ordem | E frente/atras
 *         Ctrl+D duplica | Del remove | A +imagem | F enquadra | G moldura | H personagens
 *         Ctrl+Z/Y desfaz/refaz | Ctrl+S salva | Esc volta a lista (pergunta se ha mudancas)
 */

#include "scenes/modding/stageeditor/StageEditor.h"
#include "scenes/modding/stageeditor/StageEditorIO.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "characters/Character.h"
#include "characters/Girlfriend.h"
#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

#define LIST_ROW_H 30.0f
#define ROW_H      40.0f
#define UNDO_MAX   24
#define CACHE_MAX  48

enum { PROMPT_NEW = 1, CONFIRM_LEAVE };

typedef enum { SE_PICK = 0, SE_EDIT, SE_ADD } SeMode;
typedef enum { TAB_SPRITE = 0, TAB_STAGE, TAB_COUNT } SeTab;
typedef enum { SEL_NONE = 0, SEL_SPRITE, SEL_CHAR } SelKind;
enum { CH_BF = 0, CH_GF, CH_DAD, CH_COUNT };

static const Rectangle kPicker = {200.0f, 66.0f, 880.0f, 612.0f};
static const Rectangle kLeft = {16.0f, 62.0f, 240.0f, 620.0f};
static const Rectangle kList = {24.0f, 100.0f, 224.0f, 470.0f};
static const Rectangle kCanvas = {264.0f, 62.0f, 640.0f, 620.0f};
static const Rectangle kRight = {912.0f, 62.0f, 352.0f, 620.0f};
static const Rectangle kTabs = {924.0f, 72.0f, 328.0f, 34.0f};
static const Rectangle kBtnSave = {1134.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnList = {994.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnFit = {854.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnNew = {1104.0f, 8.0f, 160.0f, 32.0f};

static const char *const kTabNames[TAB_COUNT] = {"Sprite", "Stage"};
static const char *const kCharNames[CH_COUNT] = {"BF", "GF", "Oponente"};
static const Color kCharColor[CH_COUNT] = {{110, 200, 255, 255}, {255, 140, 200, 255}, {255, 170, 90, 255}};

/* Botoes do painel esquerdo: +imagem, +retangulo, duplicar, remover, subir, descer. */
static Rectangle LeftBtn(int i)
{
    return (Rectangle){24.0f + (float)(i % 2) * 116.0f, 578.0f + (float)(i / 2) * 34.0f, 108.0f, 28.0f};
}

static float RowY(int row) { return 118.0f + (float)row * ROW_H; }
static Rectangle Wide(int row) { return (Rectangle){1020.0f, RowY(row), 232.0f, 30.0f}; }
static Rectangle Half(int row, int col) { return (Rectangle){1020.0f + (float)col * 120.0f, RowY(row), 112.0f, 30.0f}; }
static Rectangle Quarter(int row, int col) { return (Rectangle){1020.0f + (float)col * 59.0f, RowY(row), 55.0f, 30.0f}; }
static Rectangle CheckAt(int row, int col) { return (Rectangle){924.0f + (float)col * 168.0f, RowY(row), 160.0f, 30.0f}; }

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

/* Tudo que vai para o disco: base do desfazer e do "ha mudancas". Copiado so com memcpy. */
typedef struct {
    StageSprite sprites[STAGE_MAX_SPRITES];
    int count;
    StageMeta meta;
} StageDoc;

typedef struct {
    char image[128];
    bool animated;
    Texture2D tex;               /* imagem simples */
    RayAnimationHandler set;     /* animado com .xml (dono da textura) */
} Visual;

static SeMode mode;
static SeTab tab;
static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];

static StageEdEntry entries[STAGEED_LIST_MAX];
static int entryCount;
static MUiPicker stagePicker;

static char imageNames[STAGEED_LIST_MAX][MODED_NAME_MAX];
static int imageCount;
static MUiPicker imagePicker;

static StageEdEntry current;
static StageDoc doc, committed, saved;
static StageDoc undoBuf[UNDO_MAX], redoBuf[UNDO_MAX];
static int undoCount, redoCount;

static SelKind selKind;
static int selIndex;
static MUiList spriteList;
static bool lockScale = true;
static char imageBuf[128];     /* campo "Imagem": aplica no Enter/clique fora */

static Visual cache[CACHE_MAX];
static int cacheCount;
static float animTime;

static Character bf, dad;
static Girlfriend gf;
static bool charsLoaded;
static bool showChars = true;
static bool showFrame = true;

/* vista do canvas: ponto do mundo no centro + zoom */
static Vector2 camTarget;
static float camZoom = 0.5f;
static SelKind dragKind;
static Vector2 dragWorld0, dragPos0;
static bool panning;
static Vector2 panLast;

/* ------------------------------------------------------------------------- */
/* Visuais (cache de texturas por imagem)                                    */
/* ------------------------------------------------------------------------- */

static void Cache_FreeAll(void)
{
    for (int i = 0; i < cacheCount; ++i) {
        if (cache[i].set.animationCount > 0 || cache[i].set.texture.id)
            AnimationSet_FreeAll(&cache[i].set);
        else if (cache[i].tex.id)
            UnloadTexture(cache[i].tex);
    }
    memset(cache, 0, sizeof(cache));
    cacheCount = 0;
}

/* NULL = retangulo (makeGraphic). Falha de carga fica no cache (tex.id 0) para nao tentar todo frame. */
static const Visual *GetVisual(const StageSprite *s)
{
    if (s->isGraphic)
        return NULL;
    const bool animated = s->isAnimated != 0;
    for (int i = 0; i < cacheCount; ++i) {
        if (cache[i].animated == animated && strcmp(cache[i].image, s->image) == 0)
            return &cache[i];
    }
    if (cacheCount >= CACHE_MAX)
        Cache_FreeAll();

    Visual *v = &cache[cacheCount++];
    memset(v, 0, sizeof(*v));
    snprintf(v->image, sizeof(v->image), "%s", s->image);
    v->animated = animated;

    char png[MODED_ROOT_MAX + 192];
    if (!StageEd_ImagePath(current.root, s->image, png, sizeof(png)))
        return v;
    const Texture2D tex = Render_LoadTexture(png);
    if (!tex.id)
        return v;

    if (animated) {
        char xml[sizeof(png)];
        snprintf(xml, sizeof(xml), "%.*s.xml", (int)(strlen(png) - 4), png);
        if (FileExists(xml)) {
            v->set = AnimationSet_LoadAnimations(xml, tex);
            if (v->set.animationCount > 0) {
                if (!v->set.texture.id)
                    v->set.texture = tex;
                return v;
            }
            v->set.texture = (Texture2D){0};
            AnimationSet_FreeAll(&v->set);
            v->set = (RayAnimationHandler){0};
        }
    }
    v->tex = tex;
    return v;
}

static int SpriteAnim(const StageSprite *s, const Visual *v)
{
    int idx = s->animPrefix[0] ? AnimationSet_FindAnimation(&v->set, s->animPrefix) : 0;
    if (idx < 0 || idx >= v->set.animationCount)
        idx = 0;
    return idx;
}

/* Tamanho sem escala: retangulo, 1o frame da animacao ou a textura (100x100 se faltar). */
static Vector2 BaseSize(const StageSprite *s)
{
    if (s->isGraphic)
        return (Vector2){s->graphicWidth, s->graphicHeight};
    const Visual *v = GetVisual(s);
    if (v->set.animationCount > 0) {
        const RayAnimation *a = &v->set.animations[SpriteAnim(s, v)];
        if (a->frameCount > 0)
            return (Vector2){(float)a->frames[0].w, (float)a->frames[0].h};
    }
    if (v->tex.id)
        return (Vector2){(float)v->tex.width, (float)v->tex.height};
    return (Vector2){100.0f, 100.0f};
}

static Rectangle SpriteRect(int i)
{
    const StageSprite *s = &doc.sprites[i];
    const Vector2 b = BaseSize(s);
    return (Rectangle){s->x, s->y, b.x * s->scaleX, b.y * s->scaleY};
}

/* ------------------------------------------------------------------------- */
/* Personagens                                                               */
/* ------------------------------------------------------------------------- */

static float *CharPos(int c)
{
    return c == CH_BF ? doc.meta.boyfriend : (c == CH_GF ? doc.meta.girlfriend : doc.meta.opponent);
}

static const RayAnimatedObject *CharObj(int c, int *anim)
{
    const RayAnimatedObject *o = c == CH_BF ? &bf.object : (c == CH_GF ? &gf.object : &dad.object);
    *anim = c == CH_BF ? bf.animations.idle : (c == CH_GF ? gf.anims[0] : dad.animations.idle);
    if (*anim < 0 || *anim >= o->animationSet.animationCount)
        *anim = 0;
    return o;
}

static const Frame *AnimFrame(const RayAnimationHandler *set, int anim, float fps, bool *ok)
{
    *ok = false;
    if (anim < 0 || anim >= set->animationCount)
        return NULL;
    const RayAnimation *a = &set->animations[anim];
    if (a->frameCount <= 0 || !a->frames)
        return NULL;
    *ok = true;
    return &a->frames[(int)(animTime * (fps > 0.0f ? fps : 24.0f)) % a->frameCount];
}

/* Caixa do personagem no mundo (1o frame da idle), como o Render_DrawAnimatedObject. */
static Rectangle CharRect(int c)
{
    int anim;
    const RayAnimatedObject *o = CharObj(c, &anim);
    const float *p = CharPos(c);
    bool ok;
    const Frame *f = AnimFrame(&o->animationSet, anim, 0.0f, &ok);
    if (!ok)
        return (Rectangle){p[0], p[1], 300.0f, 400.0f};
    f = &o->animationSet.animations[anim].frames[0];
    const float sx = fabsf(o->scaleX), sy = o->scaleY;
    const Vector2 off = o->animationSet.animations[anim].animationOffset;
    return (Rectangle){o->position.x + p[0] - off.x - (float)f->fx * sx,
                       o->position.y + p[1] - off.y - (float)f->fy * sy, (float)f->w * sx, (float)f->h * sy};
}

static void LoadChars(void)
{
    if (charsLoaded)
        return;
    Character_SetSongDir(current.root);
    Character_Load(&bf, "bf", CHAR_BF);
    Character_Load(&dad, "dad", CHAR_DAD);
    Girlfriend_Load(&gf, "gf");
    charsLoaded = true;
}

static void FreeChars(void)
{
    if (!charsLoaded)
        return;
    AnimationSet_FreeAll(&bf.object.animationSet);
    AnimationSet_FreeAll(&dad.object.animationSet);
    AnimationSet_FreeAll(&gf.object.animationSet);
    if (bf.icon.id)
        UnloadTexture(bf.icon);
    if (dad.icon.id)
        UnloadTexture(dad.icon);
    memset(&bf, 0, sizeof(bf));
    memset(&dad, 0, sizeof(dad));
    memset(&gf, 0, sizeof(gf));
    charsLoaded = false;
}

/* ------------------------------------------------------------------------- */
/* Vista                                                                     */
/* ------------------------------------------------------------------------- */

static Vector2 ScreenToWorld(Vector2 p)
{
    return (Vector2){camTarget.x + (p.x - (kCanvas.x + kCanvas.width * 0.5f)) / camZoom,
                     camTarget.y + (p.y - (kCanvas.y + kCanvas.height * 0.5f)) / camZoom};
}

static void ZoomAt(Vector2 screen, float factor)
{
    const Vector2 before = ScreenToWorld(screen);
    camZoom = fminf(fmaxf(camZoom * factor, 0.05f), 4.0f);
    const Vector2 after = ScreenToWorld(screen);
    camTarget.x += before.x - after.x;
    camTarget.y += before.y - after.y;
}

static void Grow(Rectangle r, float *x0, float *y0, float *x1, float *y1)
{
    *x0 = fminf(*x0, r.x);
    *y0 = fminf(*y0, r.y);
    *x1 = fmaxf(*x1, r.x + r.width);
    *y1 = fmaxf(*y1, r.y + r.height);
}

/* Enquadra sprites e personagens. */
static void FitView(void)
{
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int i = 0; i < doc.count; ++i)
        Grow(SpriteRect(i), &x0, &y0, &x1, &y1);
    if (showChars) {
        for (int c = 0; c < CH_COUNT; ++c)
            Grow(CharRect(c), &x0, &y0, &x1, &y1);
    }
    if (x1 < x0) {
        camTarget = (Vector2){640.0f, 360.0f};
        camZoom = 0.5f;
        return;
    }
    const float zoom = fminf(kCanvas.width / (x1 - x0 + 200.0f), kCanvas.height / (y1 - y0 + 200.0f));
    camZoom = fminf(fmaxf(zoom, 0.05f), 2.0f);
    camTarget = (Vector2){(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
}

/* ------------------------------------------------------------------------- */
/* Documento: desfazer, selecao, operacoes                                   */
/* ------------------------------------------------------------------------- */

static bool Dirty(void)
{
    return memcmp(&doc, &saved, sizeof(doc)) != 0;
}

static StageSprite *SelSprite(void)
{
    return selKind == SEL_SPRITE && selIndex >= 0 && selIndex < doc.count ? &doc.sprites[selIndex] : NULL;
}

static void Select(SelKind kind, int index)
{
    selKind = kind;
    selIndex = index;
    if (kind == SEL_SPRITE) {
        spriteList.sel = index;
        MUi_ListShow(&spriteList, kList, LIST_ROW_H, doc.count);
        snprintf(imageBuf, sizeof(imageBuf), "%s", doc.sprites[index].image);
        tab = TAB_SPRITE;
    }
    MUi_TextBlur();
}

static void ClampSel(void)
{
    if (selKind == SEL_SPRITE && selIndex >= doc.count) {
        if (doc.count > 0)
            Select(SEL_SPRITE, doc.count - 1);
        else
            selKind = SEL_NONE;
    }
}

/* Fim do Update: guarda o estado anterior quando algo mudou e nenhum gesto esta em andamento
   (arraste, numero, digitacao), assim um arraste inteiro vira um passo so. */
static void CommitUndo(void)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || MUi_NumBusy() ||
        MUi_TextActive() || memcmp(&doc, &committed, sizeof(doc)) == 0)
        return;
    if (undoCount == UNDO_MAX) {
        memmove(&undoBuf[0], &undoBuf[1], sizeof(StageDoc) * (UNDO_MAX - 1));
        --undoCount;
    }
    memcpy(&undoBuf[undoCount++], &committed, sizeof(doc));
    memcpy(&committed, &doc, sizeof(doc));
    redoCount = 0;
}

static void Undo(bool redo)
{
    StageDoc *from = redo ? redoBuf : undoBuf;
    StageDoc *to = redo ? undoBuf : redoBuf;
    int *fromN = redo ? &redoCount : &undoCount;
    int *toN = redo ? &undoCount : &redoCount;
    if (*fromN == 0) {
        MUi_SetStatus(MUI_INFO, redo ? "nada para refazer" : "nada para desfazer");
        return;
    }
    if (*toN == UNDO_MAX) {
        memmove(&to[0], &to[1], sizeof(StageDoc) * (UNDO_MAX - 1));
        --*toN;
    }
    memcpy(&to[(*toN)++], &doc, sizeof(doc));
    memcpy(&doc, &from[--*fromN], sizeof(doc));
    memcpy(&committed, &doc, sizeof(doc));
    ClampSel();
    if (SelSprite())
        snprintf(imageBuf, sizeof(imageBuf), "%s", doc.sprites[selIndex].image);
    MUi_SetStatus(MUI_INFO, redo ? "refeito" : "desfeito");
}

static void ResetHistory(void)
{
    memcpy(&committed, &doc, sizeof(doc));
    memcpy(&saved, &doc, sizeof(doc));
    undoCount = redoCount = 0;
}

static int NewSprite(void)
{
    if (doc.count >= STAGE_MAX_SPRITES) {
        MUi_SetStatus(MUI_WARN, "limite de %d sprites", STAGE_MAX_SPRITES);
        return -1;
    }
    StageSprite *s = &doc.sprites[doc.count];
    memset(s, 0, sizeof(*s));
    s->scaleX = s->scaleY = 1.0f;
    s->scrollX = s->scrollY = 1.0f;
    s->animFps = 24.0f;
    s->animLoop = 1;
    return doc.count++;
}

static void PlaceAtView(int i)
{
    const Vector2 b = BaseSize(&doc.sprites[i]);
    doc.sprites[i].x = roundf(camTarget.x - b.x * 0.5f);
    doc.sprites[i].y = roundf(camTarget.y - b.y * 0.5f);
    Select(SEL_SPRITE, i);
}

static void AddImage(const char *image)
{
    if (!StageEd_IsImageName(image)) {
        MUi_SetStatus(MUI_WARN, "nome de imagem invalido");
        return;
    }
    const int i = NewSprite();
    if (i < 0)
        return;
    snprintf(doc.sprites[i].image, sizeof(doc.sprites[i].image), "%s", image);
    /* com .xml ao lado vira animado (primeira animacao) */
    char png[MODED_ROOT_MAX + 192];
    if (StageEd_ImagePath(current.root, image, png, sizeof(png))) {
        char xml[sizeof(png)];
        snprintf(xml, sizeof(xml), "%.*s.xml", (int)(strlen(png) - 4), png);
        doc.sprites[i].isAnimated = FileExists(xml) ? 1 : 0;
    } else {
        MUi_SetStatus(MUI_WARN, "%s.png nao encontrado em images/", image);
    }
    PlaceAtView(i);
}

static void AddRect(void)
{
    const int i = NewSprite();
    if (i < 0)
        return;
    StageSprite *s = &doc.sprites[i];
    s->isGraphic = 1;
    s->graphicWidth = 400.0f;
    s->graphicHeight = 200.0f;
    s->colorR = 120; s->colorG = 100; s->colorB = 200; s->colorA = 255;
    snprintf(s->image, sizeof(s->image), "retangulo");
    PlaceAtView(i);
}

static void Duplicate(void)
{
    const StageSprite *src = SelSprite();
    if (!src)
        return;
    const StageSprite copy = *src;
    const int i = NewSprite();
    if (i < 0)
        return;
    doc.sprites[i] = copy;
    doc.sprites[i].x += 40.0f;
    doc.sprites[i].y += 40.0f;
    Select(SEL_SPRITE, i);
}

static void RemoveSel(void)
{
    if (!SelSprite())
        return;
    memmove(&doc.sprites[selIndex], &doc.sprites[selIndex + 1], sizeof(StageSprite) * (size_t)(doc.count - selIndex - 1));
    memset(&doc.sprites[--doc.count], 0, sizeof(StageSprite));
    if (doc.count == 0)
        selKind = SEL_NONE;
    else
        Select(SEL_SPRITE, selIndex < doc.count ? selIndex : doc.count - 1);
}

/* Ordem de desenho dentro do grupo (atras/frente) segue a lista. */
static void MoveSel(int delta)
{
    if (!SelSprite())
        return;
    const int t = selIndex + delta;
    if (t < 0 || t >= doc.count)
        return;
    const StageSprite tmp = doc.sprites[t];
    doc.sprites[t] = doc.sprites[selIndex];
    doc.sprites[selIndex] = tmp;
    Select(SEL_SPRITE, t);
}

static void Nudge(float dx, float dy)
{
    StageSprite *s = SelSprite();
    if (s) {
        s->x += dx;
        s->y += dy;
    } else if (selKind == SEL_CHAR) {
        CharPos(selIndex)[0] += dx;
        CharPos(selIndex)[1] += dy;
    }
}

static void CycleSel(int delta)
{
    if (doc.count <= 0)
        return;
    const int base = SelSprite() ? selIndex : (delta > 0 ? -1 : 0);
    Select(SEL_SPRITE, (base + delta + doc.count) % doc.count);
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar                                                            */
/* ------------------------------------------------------------------------- */

static const char *StageLabel(int i, int col)
{
    if (col == 0)
        return entries[i].name;
    return col == 2 ? ModEd_RootLabel(entries[i].root) : NULL;
}

static const char *ImageLabel(int i, int col)
{
    return col == 0 ? imageNames[i] : NULL;
}

static void ToPick(void)
{
    Cache_FreeAll();
    FreeChars();
    MUi_TextBlur();
    mode = SE_PICK;
    entryCount = StageEd_ScanList(entries, STAGEED_LIST_MAX);
    MUi_PickerFilter(&stagePicker, entryCount, StageLabel);
}

static void OpenEntry(const StageEdEntry *entry)
{
    char msg[160] = {0};
    current = *entry;
    memset(&doc, 0, sizeof(doc));
    if (!StageEd_Load(&current, doc.sprites, STAGE_MAX_SPRITES, &doc.count, &doc.meta, msg, sizeof(msg))) {
        MUi_SetStatus(MUI_ERR, "nao consegui ler o stage %s", entry->name);
        return;
    }
    Cache_FreeAll();
    LoadChars();

    /* setProperty('x.scale.x') cresce do centro; o .lua gravado usa scaleObject (canto). Converte a
       posicao para o resultado ficar igual no jogo. */
    for (int i = 0; i < doc.count; ++i) {
        StageSprite *s = &doc.sprites[i];
        if (!s->scaleCenter)
            continue;
        const Vector2 b = BaseSize(s);
        s->x -= b.x * (s->scaleX - 1.0f) * 0.5f;
        s->y -= b.y * (s->scaleY - 1.0f) * 0.5f;
        s->scaleCenter = 0;
    }
    ResetHistory();

    selKind = SEL_NONE;
    spriteList = (MUiList){0};
    tab = TAB_SPRITE;
    mode = SE_EDIT;
    if (doc.count > 0)
        Select(SEL_SPRITE, 0);
    FitView();
    MUi_SetStatus(MUI_OK, "%s", msg);
}

static void TryNew(const char *name)
{
    if (!ModEd_IsSafeName(name)) {
        MUi_PromptReopen("nome invalido: use letras, digitos, - _ . (sem espacos)");
        return;
    }
    for (int i = 0; i < entryCount; ++i) {
        if (strcmp(entries[i].name, name) == 0) {
            OpenEntry(&entries[i]);
            return;
        }
    }
    StageEdEntry entry = {0};
    snprintf(entry.name, sizeof(entry.name), "%s", name);
    snprintf(entry.root, sizeof(entry.root), "%s", roots[0]);
    OpenEntry(&entry);
}

static bool SaveCurrent(void)
{
    char msg[256] = {0};
    MUi_TextBlur();
    if (!StageEd_Save(&current, doc.sprites, doc.count, &doc.meta, msg, sizeof(msg))) {
        MUi_SetStatus(MUI_ERR, "%s", msg[0] ? msg : "falha ao salvar");
        return false;
    }
    memcpy(&saved, &doc, sizeof(doc));
    MUi_SetStatus(MUI_OK, "%s", msg);
    return true;
}

static void OpenAddPicker(void)
{
    memset(&imagePicker, 0, sizeof(imagePicker));
    imageCount = StageEd_ScanImages(imageNames, STAGEED_LIST_MAX);
    MUi_PickerFilter(&imagePicker, imageCount, ImageLabel);
    MUi_TextBlur();
    mode = SE_ADD;
}

static void AskLeave(void)
{
    if (Dirty())
        MUi_ConfirmOpen(CONFIRM_LEAVE, "Voltar a lista", "Ha alteracoes nao salvas neste stage.");
    else
        ToPick();
}

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

static void HandleModal(void)
{
    int id = 0;
    const MUiModalResult res = MUi_ModalUpdate(&id);
    if (res == MUI_MODAL_NONE)
        return;
    if (id == PROMPT_NEW && res == MUI_MODAL_OK)
        TryNew(MUi_PromptText());
    else if (id == CONFIRM_LEAVE && (res == MUI_MODAL_DISCARD || (res == MUI_MODAL_OK && SaveCurrent())))
        ToPick();
}

static void Pick_Update(void)
{
    if (MUi_Button(kBtnNew) || (MUi_Ctrl() && IsKeyPressed(KEY_N))) {
        MUi_PromptOpen(PROMPT_NEW, "Novo stage", "Nome do arquivo (sem extensao), criado em stages/", stagePicker.search);
        return;
    }
    const int pick = MUi_PickerUpdate(&stagePicker, kPicker, entryCount, StageLabel);
    if (pick >= 0)
        OpenEntry(&entries[pick]);
    else if (pick == MUI_PICK_NEW)
        TryNew(stagePicker.search);
    else if (pick == MUI_PICK_BACK)
        ModdingMenu_SetScene();
}

static void Add_Update(void)
{
    if (MUi_Button(kBtnNew)) {
        mode = SE_EDIT;
        return;
    }
    const int pick = MUi_PickerUpdate(&imagePicker, kPicker, imageCount, ImageLabel);
    if (pick >= 0) {
        AddImage(imageNames[pick]);
        mode = SE_EDIT;
    } else if (pick == MUI_PICK_NEW) {
        AddImage(imagePicker.search);
        mode = SE_EDIT;
    } else if (pick == MUI_PICK_BACK) {
        mode = SE_EDIT;
    }
}

static void CheckU8(Rectangle r, uint8_t *v)
{
    bool b = *v != 0;
    if (MUi_Check(r, &b))
        *v = b ? 1 : 0;
}

static void SpriteTab_Update(void)
{
    StageSprite *s = SelSprite();
    if (!s)
        return;

    if (!s->isGraphic) {
        if (!MUi_TextIsFocused(imageBuf) && strcmp(imageBuf, s->image) != 0)
            snprintf(imageBuf, sizeof(imageBuf), "%s", s->image);
        const MUiTextEvent ev = MUi_TextField(Wide(0), imageBuf, sizeof(imageBuf));
        if (ev == MUI_TXT_SUBMIT || ev == MUI_TXT_BLUR) {
            if (StageEd_IsImageName(imageBuf))
                snprintf(s->image, sizeof(s->image), "%s", imageBuf);
            else
                MUi_SetStatus(MUI_WARN, "nome de imagem invalido");
        }
    }

    MUi_Num(Half(1, 0), &s->x, 1.0f, -100000.0f, 100000.0f);
    MUi_Num(Half(1, 1), &s->y, 1.0f, -100000.0f, 100000.0f);

    const float oldX = s->scaleX;
    if (MUi_Num(Half(2, 0), &s->scaleX, 0.01f, 0.01f, 50.0f) && lockScale && oldX > 0.0f)
        s->scaleY = fminf(fmaxf(s->scaleY * s->scaleX / oldX, 0.01f), 50.0f);
    MUi_Num(Half(2, 1), &s->scaleY, 0.01f, 0.01f, 50.0f);
    MUi_Check(CheckAt(3, 0), &lockScale);
    bool front = s->front != 0;
    if (MUi_Check(CheckAt(3, 1), &front))
        s->front = front ? 1 : 0;

    MUi_Num(Half(4, 0), &s->scrollX, 0.05f, -10.0f, 10.0f);
    MUi_Num(Half(4, 1), &s->scrollY, 0.05f, -10.0f, 10.0f);

    if (s->isGraphic) {
        MUi_Num(Half(5, 0), &s->graphicWidth, 1.0f, 1.0f, 20000.0f);
        MUi_Num(Half(5, 1), &s->graphicHeight, 1.0f, 1.0f, 20000.0f);
        uint8_t *ch[4] = {&s->colorR, &s->colorG, &s->colorB, &s->colorA};
        for (int i = 0; i < 4; ++i) {
            int v = *ch[i];
            if (MUi_NumInt(Quarter(6, i), &v, 1, 0, 255))
                *ch[i] = (uint8_t)v;
        }
    } else {
        CheckU8(CheckAt(5, 0), &s->isAnimated);
        if (s->isAnimated) {
            MUi_TextField(Wide(6), s->animPrefix, sizeof(s->animPrefix));
            MUi_Num(Half(7, 0), &s->animFps, 1.0f, 1.0f, 120.0f);
            CheckU8(CheckAt(7, 1), &s->animLoop);
        }
    }
}

static void StageTab_Update(void)
{
    StageMeta *m = &doc.meta;
    MUi_Num(Half(0, 0), &m->defaultZoom, 0.01f, 0.05f, 10.0f);
    MUi_Num(Half(1, 0), &m->cameraSpeed, 0.05f, 0.0f, 20.0f);
    CheckU8(CheckAt(2, 0), &m->isPixelStage);
    CheckU8(CheckAt(2, 1), &m->hideGirlfriend);
    float *pairs[5] = {m->boyfriend, m->girlfriend, m->opponent, m->cameraBoyfriend, m->cameraOpponent};
    for (int i = 0; i < 5; ++i) {
        MUi_Num(Half(3 + i, 0), &pairs[i][0], 1.0f, -100000.0f, 100000.0f);
        MUi_Num(Half(3 + i, 1), &pairs[i][1], 1.0f, -100000.0f, 100000.0f);
    }
    MUi_Check(CheckAt(8, 0), &showChars);
    MUi_Check(CheckAt(8, 1), &showFrame);
}

/* Clique no canvas: frente, personagens, atras (mesma ordem do desenho, de cima para baixo). */
static void HitTest(Vector2 w, SelKind *kind, int *index)
{
    *kind = SEL_NONE;
    for (int pass = 1; pass >= 0; --pass) {
        for (int i = doc.count - 1; i >= 0; --i) {
            if (doc.sprites[i].front == pass && CheckCollisionPointRec(w, SpriteRect(i))) {
                *kind = SEL_SPRITE;
                *index = i;
                return;
            }
        }
        if (pass == 1 && showChars) {
            static const int order[CH_COUNT] = {CH_BF, CH_DAD, CH_GF};
            for (int k = 0; k < CH_COUNT; ++k) {
                if (order[k] == CH_GF && doc.meta.hideGirlfriend)
                    continue;
                if (CheckCollisionPointRec(w, CharRect(order[k]))) {
                    *kind = SEL_CHAR;
                    *index = order[k];
                    return;
                }
            }
        }
    }
}

static void Canvas_Update(void)
{
    const Vector2 m = MUi_Mouse();
    const bool over = MUi_Hover(kCanvas) && MUi_MouseFree();

    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        MUi_TakeMouse();
        MUi_TextBlur();
        const Vector2 w = ScreenToWorld(m);
        SelKind kind;
        int index = 0;
        HitTest(w, &kind, &index);
        if (kind == SEL_NONE) {
            selKind = SEL_NONE;
        } else {
            Select(kind, index);
            dragKind = kind;
            dragWorld0 = w;
            dragPos0 = kind == SEL_SPRITE ? (Vector2){doc.sprites[index].x, doc.sprites[index].y}
                                          : (Vector2){CharPos(index)[0], CharPos(index)[1]};
        }
    }
    if (dragKind != SEL_NONE) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && selKind == dragKind) {
            const Vector2 w = ScreenToWorld(m);
            const float nx = roundf(dragPos0.x + w.x - dragWorld0.x);
            const float ny = roundf(dragPos0.y + w.y - dragWorld0.y);
            if (dragKind == SEL_SPRITE && SelSprite()) {
                doc.sprites[selIndex].x = nx;
                doc.sprites[selIndex].y = ny;
            } else if (dragKind == SEL_CHAR) {
                CharPos(selIndex)[0] = nx;
                CharPos(selIndex)[1] = ny;
            }
        } else {
            dragKind = SEL_NONE;
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
        StageSprite *s = SelSprite();
        if (MUi_Shift() && s) {
            const float f = 1.0f + wheel * 0.05f;
            s->scaleX = fminf(fmaxf(s->scaleX * f, 0.01f), 50.0f);
            s->scaleY = fminf(fmaxf(s->scaleY * f, 0.01f), 50.0f);
        } else {
            ZoomAt(m, wheel > 0.0f ? 1.15f : 1.0f / 1.15f);
        }
    }
}

static void Keys_Update(void)
{
    const bool ctrl = MUi_Ctrl();
    const float step = MUi_Shift() ? 10.0f : 1.0f;

    if (ctrl) {
        if (IsKeyPressed(KEY_S)) SaveCurrent();
        if (MUi_Rep(KEY_Z)) Undo(MUi_Shift());
        if (MUi_Rep(KEY_Y)) Undo(true);
        if (IsKeyPressed(KEY_D)) Duplicate();
        if (MUi_Rep(KEY_UP)) MoveSel(-1);
        if (MUi_Rep(KEY_DOWN)) MoveSel(1);
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        AskLeave();
        return;
    }
    if (MUi_Rep(KEY_LEFT)) Nudge(-step, 0.0f);
    if (MUi_Rep(KEY_RIGHT)) Nudge(step, 0.0f);
    if (MUi_Rep(KEY_UP)) Nudge(0.0f, -step);
    if (MUi_Rep(KEY_DOWN)) Nudge(0.0f, step);
    if (MUi_Rep(KEY_PAGE_UP)) CycleSel(-1);
    if (MUi_Rep(KEY_PAGE_DOWN)) CycleSel(1);
    if (IsKeyPressed(KEY_TAB)) tab = (SeTab)((tab + 1) % TAB_COUNT);
    if (IsKeyPressed(KEY_DELETE)) RemoveSel();
    if (IsKeyPressed(KEY_A)) OpenAddPicker();
    if (IsKeyPressed(KEY_F)) FitView();
    if (IsKeyPressed(KEY_G)) showFrame = !showFrame;
    if (IsKeyPressed(KEY_H)) showChars = !showChars;
    if (IsKeyPressed(KEY_E) && SelSprite())
        doc.sprites[selIndex].front = !doc.sprites[selIndex].front;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
        ZoomAt((Vector2){kCanvas.x + kCanvas.width * 0.5f, kCanvas.y + kCanvas.height * 0.5f}, 1.15f);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        ZoomAt((Vector2){kCanvas.x + kCanvas.width * 0.5f, kCanvas.y + kCanvas.height * 0.5f}, 1.0f / 1.15f);
}

static void Edit_Update(void)
{
    if (MUi_Button(kBtnSave)) SaveCurrent();
    if (MUi_Button(kBtnList)) {
        AskLeave();
        return;
    }
    if (MUi_Button(kBtnFit)) FitView();

    const int clicked = MUi_List(&spriteList, kList, LIST_ROW_H, doc.count, NULL);
    if (clicked >= 0)
        Select(SEL_SPRITE, clicked);
    if (MUi_Button(LeftBtn(0))) {
        OpenAddPicker();
        return;
    }
    if (MUi_Button(LeftBtn(1))) AddRect();
    if (MUi_Button(LeftBtn(2))) Duplicate();
    if (MUi_Button(LeftBtn(3))) RemoveSel();
    if (MUi_Button(LeftBtn(4))) MoveSel(-1);
    if (MUi_Button(LeftBtn(5))) MoveSel(1);

    const int t = MUi_Tabs(kTabs, TAB_COUNT, tab);
    if (t != (int)tab) {
        tab = (SeTab)t;
        MUi_TextBlur();
    }
    if (tab == TAB_SPRITE)
        SpriteTab_Update();
    else
        StageTab_Update();

    Canvas_Update();
    if (MUi_KeysFree())
        Keys_Update();
}

static void StageEditor_Update([[maybe_unused]] RayScene *scene)
{
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);
    MUi_FrameBegin();
    MUi_Tick(dt);
    animTime += dt;

    if (MUi_ModalActive())
        HandleModal();
    else if (mode == SE_PICK)
        Pick_Update();
    else if (mode == SE_ADD)
        Add_Update();
    else
        Edit_Update();

    if (mode != SE_PICK)
        CommitUndo();
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

/* Frame de atlas no mundo (mesma conta do Render_DrawAnimatedObject, sem scroll). */
static void DrawFrame(const RayAnimationHandler *set, int anim, const Frame *f, Vector2 pos, float sx, float sy, Color tint)
{
    const bool flip = sx < 0.0f;
    sx = fabsf(sx);
    const Vector2 off = set->animations[anim].animationOffset;
    const Rectangle src = {(float)f->x, (float)f->y, flip ? -(float)f->w : (float)f->w, (float)f->h};
    const Rectangle dst = {pos.x - off.x - (float)f->fx * sx, pos.y - off.y - (float)f->fy * sy,
                           (float)f->w * sx, (float)f->h * sy};
    DrawTexturePro(set->texture, src, dst, (Vector2){0}, 0.0f, tint);
}

static void DrawSprite(int i)
{
    const StageSprite *s = &doc.sprites[i];
    const Rectangle r = SpriteRect(i);
    if (s->isGraphic) {
        DrawRectangleRec(r, (Color){s->colorR, s->colorG, s->colorB, s->colorA});
        return;
    }
    const Visual *v = GetVisual(s);
    if (v->set.animationCount > 0) {
        const int anim = SpriteAnim(s, v);
        bool ok;
        const Frame *f = AnimFrame(&v->set, anim, s->animFps, &ok);
        if (ok)
            DrawFrame(&v->set, anim, f, (Vector2){s->x, s->y}, s->scaleX, s->scaleY, WHITE);
    } else if (v->tex.id) {
        DrawTexturePro(v->tex, (Rectangle){0, 0, (float)v->tex.width, (float)v->tex.height}, r, (Vector2){0}, 0.0f, WHITE);
    } else {
        DrawRectangleRec(r, (Color){255, 60, 80, 40});
        DrawRectangleLinesEx(r, 2.0f / camZoom, MUI_C_BAD);
    }
}

static void DrawChar(int c)
{
    if (c == CH_GF && doc.meta.hideGirlfriend)
        return;
    int anim;
    const RayAnimatedObject *o = CharObj(c, &anim);
    bool ok;
    const Frame *f = AnimFrame(&o->animationSet, anim, (float)(o->animationSet.animationCount > 0 ? o->animationSet.animations[anim].fps : 24), &ok);
    const float *p = CharPos(c);
    if (ok)
        DrawFrame(&o->animationSet, anim, f, (Vector2){o->position.x + p[0], o->position.y + p[1]}, o->scaleX, o->scaleY, (Color){255, 255, 255, 200});
    const Rectangle r = CharRect(c);
    DrawRectangleLinesEx(r, 1.5f / camZoom, ColorAlpha(kCharColor[c], 0.7f));
}

static void DrawWorldGrid(void)
{
    const Vector2 a = ScreenToWorld((Vector2){kCanvas.x, kCanvas.y});
    const Vector2 b = ScreenToWorld((Vector2){kCanvas.x + kCanvas.width, kCanvas.y + kCanvas.height});
    const float step = camZoom < 0.15f ? 500.0f : 100.0f;
    const Color line = {255, 255, 255, 14};
    for (float x = floorf(a.x / step) * step; x <= b.x; x += step)
        DrawLineEx((Vector2){x, a.y}, (Vector2){x, b.y}, 1.0f / camZoom, x == 0.0f ? (Color){255, 255, 255, 50} : line);
    for (float y = floorf(a.y / step) * step; y <= b.y; y += step)
        DrawLineEx((Vector2){a.x, y}, (Vector2){b.x, y}, 1.0f / camZoom, y == 0.0f ? (Color){255, 255, 255, 50} : line);
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
    DrawWorldGrid();
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < doc.count; ++i) {
            if (doc.sprites[i].front == pass)
                DrawSprite(i);
        }
        if (pass == 0 && showChars) {
            DrawChar(CH_GF);
            DrawChar(CH_DAD);
            DrawChar(CH_BF);
        }
    }

    const float lw = 2.0f / camZoom;
    if (SelSprite()) {
        DrawRectangleLinesEx(SpriteRect(selIndex), lw, MUI_C_ACCENT);
    } else if (selKind == SEL_CHAR) {
        DrawRectangleLinesEx(CharRect(selIndex), lw, MUI_C_ACCENT);
    }
    if (showFrame && doc.meta.defaultZoom > 0.01f) {
        const float w = 1280.0f / doc.meta.defaultZoom, h = 720.0f / doc.meta.defaultZoom;
        DrawRectangleLinesEx((Rectangle){camTarget.x - w * 0.5f, camTarget.y - h * 0.5f, w, h}, lw, MUI_C_HINT);
    }
    EndMode2D();

    /* nomes dos personagens em tamanho de tela */
    if (showChars) {
        for (int c = 0; c < CH_COUNT; ++c) {
            if (c == CH_GF && doc.meta.hideGirlfriend)
                continue;
            const Rectangle r = CharRect(c);
            const Vector2 sp = GetWorldToScreen2D((Vector2){r.x, r.y}, cam);
            MUi_TextShadow(kCharNames[c], sp.x + 4.0f, sp.y + 4.0f, 16.0f, kCharColor[c]);
        }
    }
    MUi_ClipEnd();

    char line[96];
    snprintf(line, sizeof(line), "vista %d%%   %s%s", (int)(camZoom * 100.0f + 0.5f),
             showFrame ? "moldura = tela no zoom padrao (G)" : "", "");
    MUi_TextShadow(line, kCanvas.x + 8.0f, kCanvas.y + kCanvas.height - 24.0f, 15.0f, MUI_C_DIM);
    DrawRectangleLinesEx(kCanvas, 1.0f, MUI_C_EDGE);
}

static void DrawSpriteList(void)
{
    MUi_DrawPanel(kLeft);
    MUi_DrawSection(kLeft.x + 10.0f, kLeft.y + 10.0f, "Sprites");
    MUi_DrawListBegin(kList);
    const int rows = MUi_ListRows(kList, LIST_ROW_H);
    for (int i = spriteList.top; i < doc.count && i <= spriteList.top + rows; ++i) {
        const StageSprite *s = &doc.sprites[i];
        const Rectangle row = MUi_ListRow(&spriteList, kList, LIST_ROW_H, i);
        MUi_DrawListRow(row, selKind == SEL_SPRITE && i == selIndex);
        const bool missing = !s->isGraphic && GetVisual(s)->tex.id == 0 && GetVisual(s)->set.animationCount == 0;
        MUi_Text(s->isGraphic ? "R" : (s->isAnimated ? "A" : "I"), row.x + 6.0f, row.y + 6.0f, 16.0f, MUI_C_FAINT);
        MUi_ClipBegin((Rectangle){row.x + 22.0f, row.y, row.width - 60.0f, row.height});
        MUi_Text(s->image, row.x + 24.0f, row.y + 6.0f, 17.0f, missing ? MUI_C_BAD : MUI_C_TEXT);
        MUi_ClipEnd();
        if (s->front)
            MUi_TextRight("frente", row.x + row.width - 8.0f, row.y + 7.0f, 14.0f, MUI_C_HINT);
    }
    if (doc.count == 0)
        MUi_TextCenter("vazio: A adiciona imagem", (Rectangle){kList.x, kList.y + 10.0f, kList.width, 24.0f}, 16.0f, MUI_C_FAINT);
    MUi_DrawListEnd(&spriteList, kList, LIST_ROW_H, doc.count);

    const bool has = SelSprite() != NULL;
    MUi_DrawButton(LeftBtn(0), "+ Imagem", MUI_BTN_ACCENT);
    MUi_DrawButton(LeftBtn(1), "+ Retangulo", MUI_BTN_NORMAL);
    MUi_DrawButton(LeftBtn(2), "Duplicar", has ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(LeftBtn(3), "Remover", has ? MUI_BTN_DANGER : MUI_BTN_OFF);
    MUi_DrawButton(LeftBtn(4), "Subir", has && selIndex > 0 ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(LeftBtn(5), "Descer", has && selIndex + 1 < doc.count ? MUI_BTN_NORMAL : MUI_BTN_OFF);
}

static void Label(int row, const char *text)
{
    MUi_Text(text, 924.0f, RowY(row) + 6.0f, 17.0f, MUI_C_DIM);
}

static void DrawSpriteTab(void)
{
    const StageSprite *s = SelSprite();
    if (!s) {
        if (selKind == SEL_CHAR) {
            MUi_Text(kCharNames[selIndex], 924.0f, RowY(0) + 4.0f, 20.0f, kCharColor[selIndex]);
            MUi_Text("arraste no canvas ou edite na aba Stage", 924.0f, RowY(1), 16.0f, MUI_C_FAINT);
        } else {
            MUi_Text("nada selecionado", 924.0f, RowY(0) + 4.0f, 18.0f, MUI_C_FAINT);
        }
        return;
    }
    if (s->isGraphic) {
        Label(0, "Retangulo");
        MUi_Text("makeGraphic", 1020.0f, RowY(0) + 6.0f, 17.0f, MUI_C_FAINT);
    } else {
        Label(0, "Imagem");
        MUi_DrawTextField(Wide(0), imageBuf, "pasta/nome em images/");
    }
    Label(1, "Posicao");
    MUi_DrawNum(Half(1, 0), s->x, 1.0f);
    MUi_DrawNum(Half(1, 1), s->y, 1.0f);
    Label(2, "Escala");
    MUi_DrawNum(Half(2, 0), s->scaleX, 0.01f);
    MUi_DrawNum(Half(2, 1), s->scaleY, 0.01f);
    MUi_DrawCheck(CheckAt(3, 0), "Proporcional", lockScale);
    MUi_DrawCheck(CheckAt(3, 1), "Na frente (E)", s->front != 0);
    Label(4, "Scroll");
    MUi_DrawNum(Half(4, 0), s->scrollX, 0.05f);
    MUi_DrawNum(Half(4, 1), s->scrollY, 0.05f);

    if (s->isGraphic) {
        Label(5, "Tamanho");
        MUi_DrawNum(Half(5, 0), s->graphicWidth, 1.0f);
        MUi_DrawNum(Half(5, 1), s->graphicHeight, 1.0f);
        Label(6, "RGBA");
        const uint8_t ch[4] = {s->colorR, s->colorG, s->colorB, s->colorA};
        for (int i = 0; i < 4; ++i)
            MUi_DrawNum(Quarter(6, i), (float)ch[i], 1.0f);
        MUi_DrawSwatch((Rectangle){924.0f, RowY(7), 328.0f, 24.0f}, (Color){s->colorR, s->colorG, s->colorB, 255}, false);
    } else {
        MUi_DrawCheck(CheckAt(5, 0), "Animado (.xml)", s->isAnimated != 0);
        if (s->isAnimated) {
            Label(6, "Prefixo");
            MUi_DrawTextField(Wide(6), s->animPrefix, "vazio = 1a animacao");
            Label(7, "FPS");
            MUi_DrawNum(Half(7, 0), s->animFps, 1.0f);
            MUi_DrawCheck(CheckAt(7, 1), "Loop", s->animLoop != 0);
        }
        const Visual *v = GetVisual(s);
        const Vector2 b = BaseSize(s);
        char info[96];
        if (!v->tex.id && v->set.animationCount == 0)
            snprintf(info, sizeof(info), "%s.png nao encontrado", s->image);
        else if (s->isAnimated && v->set.animationCount == 0)
            snprintf(info, sizeof(info), "sem .xml: aparece parado no jogo");
        else
            snprintf(info, sizeof(info), "%.0f x %.0f px (%.0f x %.0f na tela)", b.x, b.y, b.x * s->scaleX, b.y * s->scaleY);
        MUi_Text(info, 924.0f, RowY(8) + 8.0f, 16.0f, v->tex.id || v->set.animationCount ? MUI_C_FAINT : MUI_C_BAD);
    }
    if (s->scrollX == 0.0f && s->scrollY == 0.0f && !s->isGraphic)
        MUi_Text("scroll 0,0: no jogo cobre a tela inteira", 924.0f, RowY(9) + 8.0f, 16.0f, MUI_C_HINT);
    MUi_Text("arraste o numero, roda ou clique e digite", 924.0f, kRight.y + kRight.height - 28.0f, 15.0f, MUI_C_FAINT);
}

static void DrawStageTab(void)
{
    const StageMeta *m = &doc.meta;
    Label(0, "Zoom");
    MUi_DrawNum(Half(0, 0), m->defaultZoom, 0.01f);
    Label(1, "Vel. cam.");
    MUi_DrawNum(Half(1, 0), m->cameraSpeed, 0.05f);
    MUi_DrawCheck(CheckAt(2, 0), "Pixel stage", m->isPixelStage != 0);
    MUi_DrawCheck(CheckAt(2, 1), "Sem GF", m->hideGirlfriend != 0);
    static const char *const names[5] = {"BF", "GF", "Oponente", "Cam. BF", "Cam. opon."};
    const float *pairs[5] = {m->boyfriend, m->girlfriend, m->opponent, m->cameraBoyfriend, m->cameraOpponent};
    for (int i = 0; i < 5; ++i) {
        Label(3 + i, names[i]);
        MUi_DrawNum(Half(3 + i, 0), pairs[i][0], 1.0f);
        MUi_DrawNum(Half(3 + i, 1), pairs[i][1], 1.0f);
    }
    MUi_DrawCheck(CheckAt(8, 0), "Personagens (H)", showChars);
    MUi_DrawCheck(CheckAt(8, 1), "Moldura (G)", showFrame);
    MUi_Text("Cam.: deslocamento do foco da camera", 924.0f, RowY(9) + 8.0f, 15.0f, MUI_C_FAINT);
}

static void Edit_Draw(void)
{
    char detail[MODED_NAME_MAX + MODED_ROOT_MAX + 8];
    snprintf(detail, sizeof(detail), "%s  [%s]", current.name, ModEd_RootLabel(current.root));
    MUi_DrawTopBar("Stage Editor", detail, Dirty());
    MUi_DrawButton(kBtnFit, "Enquadrar (F)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnList, "Lista (Esc)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnSave, "Salvar", Dirty() ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);

    DrawCanvas();
    DrawSpriteList();
    MUi_DrawPanel(kRight);
    MUi_DrawTabs(kTabs, kTabNames, TAB_COUNT, tab);
    if (tab == TAB_SPRITE)
        DrawSpriteTab();
    else
        DrawStageTab();

    MUi_DrawBottomBar("setas move (Shift x10)  PgUp/PgDn sel.  Ctrl+Up/Dn ordem  E frente  Ctrl+D dup.  Del  A +img  "
                      "dir. arrasta vista  roda zoom  Shift+roda escala  Ctrl+Z/Y  Ctrl+S");
}

static void StageEditor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    if (mode == SE_PICK) {
        MUi_DrawTopBar("Stage Editor", "escolha um stage", false);
        MUi_DrawButton(kBtnNew, "Novo (Ctrl+N)", MUI_BTN_ACCENT);
        MUi_DrawPicker(&stagePicker, kPicker, StageLabel, "buscar stage...",
                       entryCount ? "nenhum stage com esse nome: Enter cria um novo" : "nenhum stage: digite um nome e Enter");
        MUi_DrawBottomBar("digite para buscar   Enter/clique abre   Ctrl+N novo   Esc volta");
    } else if (mode == SE_ADD) {
        MUi_DrawTopBar("Stage Editor", "adicionar imagem (images/)", Dirty());
        MUi_DrawButton(kBtnNew, "Cancelar", MUI_BTN_NORMAL);
        MUi_DrawPicker(&imagePicker, kPicker, ImageLabel, "buscar imagem...",
                       "nenhuma imagem com esse nome: Enter usa o nome digitado");
        MUi_DrawBottomBar("digite para buscar   Enter/clique adiciona   Esc cancela");
    } else {
        Edit_Draw();
    }
    MUi_DrawModal();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void StageEditor_Create([[maybe_unused]] RayScene *scene)
{
    MUi_Reset();
    if (ModEd_Roots(roots, MODED_ROOTS_MAX) == 0)
        snprintf(roots[0], sizeof(roots[0]), "assets");
    memset(&doc, 0, sizeof(doc));
    memset(&current, 0, sizeof(current));
    memset(&stagePicker, 0, sizeof(stagePicker));
    ResetHistory();
    selKind = dragKind = SEL_NONE;
    panning = false;
    animTime = 0.0f;
    camTarget = (Vector2){640.0f, 360.0f};
    camZoom = 0.5f;
    ToPick();
}

static void StageEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    Cache_FreeAll();
    FreeChars();
    MUi_Reset();
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
