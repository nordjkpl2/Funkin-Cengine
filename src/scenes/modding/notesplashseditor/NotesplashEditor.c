/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NotesplashEditor.c - lista de notesplashes: escolhe o atlas Sparrow (images/ dos mods e de assets,
 * ou qualquer caminho digitado em "Abrir"), a animacao e a cor; cria/remove entradas e define a
 * ativa usada na gameplay. Gravacao: assets/notesplashes.json (Ctrl+S).
 *
 * Controles: clique nas listas (setas navegam a ultima lista clicada), Ctrl+O abre atlas por
 * caminho, Ctrl+S salva, Del remove entrada, Esc volta (pergunta se ha mudancas).
 */

#include "scenes/modding/notesplashseditor/NotesplashEditor.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "data/config/NotesplashConfig.h"
#include "data/config/EffectsConfig.h"
#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHEETS_MAX    128
#define PREFIXES_MAX  128
#define SPLASH_PERIOD 0.6f

#define ENTRY_ROW_H 46.0f
#define LIST_ROW_H  26.0f

enum { PROMPT_OPEN = 1, CONFIRM_LEAVE };
enum { LIST_ENTRIES = 0, LIST_SHEETS, LIST_ANIMS };

static const Rectangle kLeft = {16.0f, 62.0f, 300.0f, 620.0f};
static const Rectangle kMid = {328.0f, 62.0f, 380.0f, 620.0f};
static const Rectangle kRight = {720.0f, 62.0f, 544.0f, 620.0f};
static const Rectangle kBtnSave = {1134.0f, 8.0f, 130.0f, 32.0f};
static const Rectangle kBtnOpen = {964.0f, 8.0f, 160.0f, 32.0f};

typedef struct {
    char path[NOTESPLASH_PATH_MAX];  /* xml do atlas */
    char label[64];                  /* nome do arquivo sem extensao */
    char root[40];                   /* rotulo da raiz (mod ou assets) */
} Sheet;

static NotesplashList draft;
static Sheet sheets[SHEETS_MAX];
static int sheetCount;
static int visible[SHEETS_MAX];
static int visibleCount;
static char search[48];

static char prefixes[PREFIXES_MAX][NOTESPLASH_PREFIX_MAX];
static int prefixCount;
static char prefixesOf[NOTESPLASH_PATH_MAX];

static MUiList entryList, sheetList, animList;
static int activeList;
static bool dirty;

static RayAnimationHandler previewSet;
static char previewLoaded[NOTESPLASH_PATH_MAX];
static int previewAnim = -1;
static RayAnimatedObject previewObj;
static float previewT;
static bool previewStale;

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

static Rectangle EntryListRect(void) { return (Rectangle){kLeft.x + 12.0f, kLeft.y + 42.0f, kLeft.width - 24.0f, 460.0f}; }
static Rectangle BtnCopy(void) { return (Rectangle){kLeft.x + 12.0f, kLeft.y + 514.0f, 134.0f, 34.0f}; }
static Rectangle BtnRemove(void) { return (Rectangle){kLeft.x + 154.0f, kLeft.y + 514.0f, 134.0f, 34.0f}; }
static Rectangle BtnActive(void) { return (Rectangle){kLeft.x + 12.0f, kLeft.y + 560.0f, kLeft.width - 24.0f, 44.0f}; }
static Rectangle SearchRect(void) { return (Rectangle){kMid.x + 12.0f, kMid.y + 40.0f, kMid.width - 24.0f, 32.0f}; }
static Rectangle SheetListRect(void) { return (Rectangle){kMid.x + 12.0f, kMid.y + 80.0f, kMid.width - 24.0f, 250.0f}; }
static Rectangle AnimListRect(void) { return (Rectangle){kMid.x + 12.0f, kMid.y + 374.0f, kMid.width - 24.0f, 232.0f}; }
static Rectangle PreviewRect(void) { return (Rectangle){kRight.x + 12.0f, kRight.y + 40.0f, kRight.width - 24.0f, 400.0f}; }

static Rectangle SwatchRect(int slot)
{
    return (Rectangle){kRight.x + 16.0f + (float)slot * 70.0f, kRight.y + 490.0f, 58.0f, 40.0f};
}

/* ------------------------------------------------------------------ */
/* Atlas                                                               */
/* ------------------------------------------------------------------ */

/* So o comeco do arquivo: e um atlas Sparrow? */
static bool IsSparrow(const char *path)
{
    char head[2048];
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    const size_t n = fread(head, 1, sizeof(head) - 1, f);
    fclose(f);
    head[n] = '\0';
    return strstr(head, "<TextureAtlas") != NULL;
}

static int FindSheet(const char *path)
{
    for (int i = 0; i < sheetCount; ++i) {
        if (strcmp(sheets[i].path, path) == 0)
            return i;
    }
    return -1;
}

static int AddSheet(const char *path, const char *rootLabel)
{
    const int known = FindSheet(path);
    if (known >= 0 || sheetCount >= SHEETS_MAX || !IsSparrow(path))
        return known;

    Sheet *s = &sheets[sheetCount];
    snprintf(s->path, sizeof(s->path), "%s", path);
    const char *base = strrchr(path, '/');
    snprintf(s->label, sizeof(s->label), "%s", base ? base + 1 : path);
    char *dot = strrchr(s->label, '.');
    if (dot)
        *dot = '\0';
    snprintf(s->root, sizeof(s->root), "%s", rootLabel);
    return sheetCount++;
}

static void ScanSheets(void)
{
    static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
    const int rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);

    sheetCount = 0;
    for (int r = 0; r < rootCount; ++r) {
        char dir[MODED_ROOT_MAX + 16];
        snprintf(dir, sizeof(dir), "%s/images", roots[r]);
        if (!DirectoryExists(dir))
            continue;
        FilePathList files = LoadDirectoryFilesEx(dir, ".xml", false);
        for (unsigned int i = 0; i < files.count && sheetCount < SHEETS_MAX; ++i) {
            char path[NOTESPLASH_PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dir, GetFileName(files.paths[i]));
            AddSheet(path, ModEd_RootLabel(roots[r]));
        }
        UnloadDirectoryFiles(files);
    }
    /* atlas das entradas que estao fora dessas pastas */
    for (int i = 0; i < draft.count; ++i)
        AddSheet(draft.entries[i].animset, "json");
}

static void FilterSheets(void)
{
    visibleCount = 0;
    for (int i = 0; i < sheetCount; ++i) {
        if (ModEd_ContainsCI(sheets[i].label, search) || ModEd_ContainsCI(sheets[i].root, search))
            visible[visibleCount++] = i;
    }
}

static void AddPrefix(const char *pre)
{
    for (int i = 0; i < prefixCount; ++i) {
        if (strcmp(prefixes[i], pre) == 0)
            return;
    }
    if (prefixCount < PREFIXES_MAX)
        snprintf(prefixes[prefixCount++], NOTESPLASH_PREFIX_MAX, "%s", pre);
}

/* Prefixos das animacoes (nome do SubTexture sem os digitos finais). So relê quando o atlas muda. */
static void LoadPrefixes(const char *path)
{
    if (strcmp(prefixesOf, path) == 0)
        return;
    snprintf(prefixesOf, sizeof(prefixesOf), "%s", path);
    prefixCount = 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);
    char *buf = size > 0 ? malloc((size_t)size + 1) : NULL;
    if (!buf) {
        fclose(f);
        return;
    }
    const size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    for (const char *p = buf; (p = strstr(p, "<SubTexture")) != NULL;) {
        const char *tagEnd = strchr(p, '>');
        const char *nm = strstr(p, "name=\"");
        if (!nm || (tagEnd && nm > tagEnd)) {
            p += 11;
            continue;
        }
        nm += 6;
        const char *q = strchr(nm, '"');
        if (!q)
            break;
        size_t len = (size_t)(q - nm);
        while (len > 0 && nm[len - 1] >= '0' && nm[len - 1] <= '9')
            --len;
        if (len > 0 && len < NOTESPLASH_PREFIX_MAX) {
            char pre[NOTESPLASH_PREFIX_MAX];
            memcpy(pre, nm, len);
            pre[len] = '\0';
            AddPrefix(pre);
        }
        p = q;
    }
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Entrada atual                                                       */
/* ------------------------------------------------------------------ */

static NotesplashEntry *Current(void)
{
    return &draft.entries[entryList.sel];
}

/* Listas do meio seguem a entrada: atlas e animacao dela selecionados. */
static void SyncLists(void)
{
    const NotesplashEntry *e = Current();
    LoadPrefixes(e->animset);

    const int sheet = FindSheet(e->animset);
    sheetList.sel = 0;
    for (int i = 0; i < visibleCount; ++i) {
        if (visible[i] == sheet)
            sheetList.sel = i;
    }
    MUi_ListShow(&sheetList, SheetListRect(), LIST_ROW_H, visibleCount);

    animList.sel = 0;
    for (int i = 0; i < prefixCount; ++i) {
        if (strcmp(prefixes[i], e->prefix) == 0)
            animList.sel = i;
    }
    MUi_ListShow(&animList, AnimListRect(), LIST_ROW_H, prefixCount);
    previewStale = true;
}

static void SelectEntry(int i)
{
    if (i < 0 || i >= draft.count)
        return;
    entryList.sel = i;
    MUi_ListShow(&entryList, EntryListRect(), ENTRY_ROW_H, draft.count);
    SyncLists();
}

/* Troca o atlas da entrada; mantem a animacao se o novo atlas tiver o mesmo prefixo. */
static void SetSheet(int sheet)
{
    if (sheet < 0 || sheet >= sheetCount)
        return;
    NotesplashEntry *e = Current();
    if (strcmp(e->animset, sheets[sheet].path) == 0)
        return;
    snprintf(e->animset, sizeof(e->animset), "%s", sheets[sheet].path);
    LoadPrefixes(e->animset);

    bool keep = false;
    for (int i = 0; i < prefixCount && !keep; ++i)
        keep = strcmp(prefixes[i], e->prefix) == 0;
    if (!keep && prefixCount > 0)
        snprintf(e->prefix, sizeof(e->prefix), "%s", prefixes[0]);
    dirty = true;
    SyncLists();
}

static void SetPrefix(int i)
{
    if (i < 0 || i >= prefixCount)
        return;
    NotesplashEntry *e = Current();
    if (strcmp(e->prefix, prefixes[i]) == 0)
        return;
    snprintf(e->prefix, sizeof(e->prefix), "%s", prefixes[i]);
    dirty = true;
    previewStale = true;
}

static void CopyEntry(void)
{
    if (draft.count >= NOTESPLASH_MAX) {
        MUi_SetStatus(MUI_WARN, "limite de %d entradas", NOTESPLASH_MAX);
        return;
    }
    draft.entries[draft.count] = *Current();
    draft.count++;
    dirty = true;
    SelectEntry(draft.count - 1);
}

static void RemoveEntry(void)
{
    if (draft.count <= 1) {
        MUi_SetStatus(MUI_WARN, "precisa ter ao menos 1 entrada");
        return;
    }
    const int e = entryList.sel;
    for (int i = e; i < draft.count - 1; ++i)
        draft.entries[i] = draft.entries[i + 1];
    draft.count--;
    if (draft.active == e)
        draft.active = 0;
    else if (draft.active > e)
        draft.active--;
    dirty = true;
    SelectEntry(e >= draft.count ? draft.count - 1 : e);
}

static void SetActive(void)
{
    if (draft.active == entryList.sel)
        return;
    draft.active = entryList.sel;
    dirty = true;
    MUi_SetStatus(MUI_INFO, "entrada %d vai ser usada na gameplay (salve com Ctrl+S)", draft.active + 1);
}

/* tint: -1 = padrao do Effects Editor; 0..EFFECTS_TINTS-1 */
static void SetTint(int tint)
{
    if (Current()->tint == tint)
        return;
    Current()->tint = tint;
    dirty = true;
}

static bool SaveNow(void)
{
    NotesplashConfig_Set(&draft);
    if (!NotesplashConfig_Save()) {
        MUi_SetStatus(MUI_ERR, "falha ao gravar assets/notesplashes.json");
        return false;
    }
    dirty = false;
    MUi_SetStatus(MUI_OK, "gravado em assets/notesplashes.json");
    return true;
}

/* Caminho digitado: .xml ou .png (troca para .xml) de qualquer pasta. */
static void OpenTypedPath(const char *typed)
{
    char path[NOTESPLASH_PATH_MAX];
    snprintf(path, sizeof(path), "%s", typed);
    ModEd_CleanPath(path);
    if (!path[0]) {
        MUi_PromptReopen("digite um caminho");
        return;
    }

    const char *ext = ModEd_Ext(path);
    if (ext[0] == '\0') {
        strncat(path, ".xml", sizeof(path) - strlen(path) - 1);
    } else if (strcmp(ext, ".png") == 0 || strcmp(ext, ".PNG") == 0) {
        char *dot = strrchr(path, '.');
        snprintf(dot, sizeof(path) - (size_t)(dot - path), ".xml");
    }

    char msg[NOTESPLASH_PATH_MAX + 64];
    if (!FileExists(path)) {
        snprintf(msg, sizeof(msg), "nao achei: %s", path);
        MUi_PromptReopen(msg);
        return;
    }
    char png[NOTESPLASH_PATH_MAX];
    if (!NotesplashConfig_TexturePath(path, png, sizeof(png)) || !FileExists(png)) {
        snprintf(msg, sizeof(msg), "falta o .png do atlas ao lado do .xml");
        MUi_PromptReopen(msg);
        return;
    }
    const int sheet = AddSheet(path, "aberto");
    if (sheet < 0) {
        MUi_PromptReopen(sheetCount >= SHEETS_MAX ? "lista de atlas cheia" : "nao e um atlas Sparrow (TextureAtlas)");
        return;
    }
    search[0] = '\0';
    FilterSheets();
    SetSheet(sheet);
    SyncLists();
    MUi_SetStatus(MUI_OK, "atlas aberto: %s", path);
}

static void Leave(void)
{
    ModdingMenu_SetScene();
}

/* ------------------------------------------------------------------ */
/* Preview                                                             */
/* ------------------------------------------------------------------ */

static void FreePreview(void)
{
    AnimationSet_FreeAll(&previewSet);
    previewLoaded[0] = '\0';
    previewAnim = -1;
}

static void RefreshPreview(void)
{
    previewStale = false;
    const NotesplashEntry *e = Current();
    if (strcmp(previewLoaded, e->animset) != 0) {
        FreePreview();
        char texture[NOTESPLASH_PATH_MAX];
        if (NotesplashConfig_TexturePath(e->animset, texture, sizeof(texture)) && FileExists(texture)) {
            previewSet = AnimationSet_LoadAnimations(e->animset, Render_LoadTexture(texture));
            snprintf(previewLoaded, sizeof(previewLoaded), "%s", e->animset);
        }
        Render_DefaultAnimated(&previewObj);
        previewObj.animationSet = previewSet;
    }
    previewAnim = previewLoaded[0] ? AnimationSet_FindAnimation(&previewSet, e->prefix) : -1;
    if (previewAnim >= 0) {
        AnimationSet_SetAnimationData(&previewSet, previewAnim, EffectsConfig_Get()->splash.fps, 0);
        previewObj.animationSet = previewSet;
        AnimatedObject_SetAnimation(&previewObj, previewAnim);
    }
    previewT = 0.0f;
}

static void UpdatePreview(float dt)
{
    if (previewStale)
        RefreshPreview();
    previewT += dt;
    if (previewT >= SPLASH_PERIOD) {
        previewT = 0.0f;
        if (previewAnim >= 0)
            AnimatedObject_SetAnimation(&previewObj, previewAnim);
    }
    if (previewAnim < 0)
        return;

    /* Mesma conta da gameplay: centro do receptor + escala/offset/opacidade do Effects Editor. */
    const EffectsSplash *cfg = &EffectsConfig_Get()->splash;
    const Rectangle area = PreviewRect();
    const float cx = area.x + area.width * 0.5f;
    const float cy = area.y + area.height * 0.5f;
    const float scale = (float)cfg->scalePct / 100.0f;
    const int entryTint = Current()->tint;
    const Color tint = EffectsConfig_TintColor(entryTint >= 0 ? entryTint : cfg->tint);

    previewObj.scaleX = previewObj.scaleY = scale;
    previewObj.color = (Color){tint.r, tint.g, tint.b, (unsigned char)(cfg->alphaPct * 255 / 100)};
    AnimatedObject_UpdateFrame(&previewObj);
    const Vector2 size = AnimatedObject_Sizes(&previewObj);
    previewObj.position.x = cx - size.x * scale * 0.5f + (float)cfg->offsetX;
    previewObj.position.y = cy - size.y * scale * 0.5f + (float)cfg->offsetY;
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void HandleModal(void)
{
    int id = 0;
    const MUiModalResult res = MUi_ModalUpdate(&id);
    if (id == PROMPT_OPEN && res == MUI_MODAL_OK) {
        OpenTypedPath(MUi_PromptText());
    } else if (id == CONFIRM_LEAVE) {
        if (res == MUI_MODAL_OK && SaveNow())
            Leave();
        else if (res == MUI_MODAL_DISCARD)
            Leave();
    }
}

static void Editor_Update([[maybe_unused]] RayScene *scene)
{
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);
    MUi_FrameBegin();
    MUi_Tick(dt);
    UpdatePreview(dt);

    if (MUi_ModalActive()) {
        HandleModal();
        return;
    }

    /* barra de cima */
    if (MUi_Button(kBtnSave))
        SaveNow();
    if (MUi_Button(kBtnOpen))
        MUi_PromptOpen(PROMPT_OPEN, "Abrir atlas", "Caminho do .xml (ou .png) Sparrow. Ex.: assets/mods/MeuMod/images/splash.xml", "");

    /* entradas */
    const int clickedEntry = MUi_List(&entryList, EntryListRect(), ENTRY_ROW_H, draft.count, NULL);
    if (clickedEntry >= 0) {
        activeList = LIST_ENTRIES;
        SelectEntry(clickedEntry);
    }
    if (MUi_Button(BtnCopy()))
        CopyEntry();
    if (MUi_Button(BtnRemove()))
        RemoveEntry();
    if (MUi_Button(BtnActive()))
        SetActive();

    /* atlas e animacoes */
    const MUiTextEvent ev = MUi_TextField(SearchRect(), search, sizeof(search));
    if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL) {
        FilterSheets();
        SyncLists();
    }
    const int clickedSheet = MUi_List(&sheetList, SheetListRect(), LIST_ROW_H, visibleCount, NULL);
    if (clickedSheet >= 0) {
        activeList = LIST_SHEETS;
        SetSheet(visible[clickedSheet]);
    }
    const int clickedAnim = MUi_List(&animList, AnimListRect(), LIST_ROW_H, prefixCount, NULL);
    if (clickedAnim >= 0) {
        activeList = LIST_ANIMS;
        SetPrefix(clickedAnim);
    }

    /* cor */
    for (int slot = 0; slot <= EFFECTS_TINTS; ++slot) {
        if (MUi_Button(SwatchRect(slot)))
            SetTint(slot - 1);
    }

    if (!MUi_KeysFree())
        return;

    if (MUi_Ctrl() && IsKeyPressed(KEY_S)) {
        SaveNow();
        return;
    }
    if (MUi_Ctrl() && IsKeyPressed(KEY_O)) {
        MUi_PromptOpen(PROMPT_OPEN, "Abrir atlas", "Caminho do .xml (ou .png) Sparrow. Ex.: assets/mods/MeuMod/images/splash.xml", "");
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_LEAVE, "Sair do Notesplash Editor", "Ha alteracoes nao gravadas.");
        else
            Leave();
        return;
    }
    if (IsKeyPressed(KEY_DELETE))
        RemoveEntry();

    if (activeList == LIST_ENTRIES) {
        if (MUi_ListKeys(&entryList, EntryListRect(), ENTRY_ROW_H, draft.count))
            SelectEntry(entryList.sel);
    } else if (activeList == LIST_SHEETS) {
        if (MUi_ListKeys(&sheetList, SheetListRect(), LIST_ROW_H, visibleCount) && visibleCount > 0)
            SetSheet(visible[sheetList.sel]);
    } else if (MUi_ListKeys(&animList, AnimListRect(), LIST_ROW_H, prefixCount)) {
        SetPrefix(animList.sel);
    }
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void SectionTitle(Rectangle panel, float dy, const char *title, int list)
{
    MUi_DrawSection(panel.x + 14.0f, panel.y + dy, title);
    if (list >= 0 && activeList == list)
        MUi_Text("setas", panel.x + panel.width - 62.0f, panel.y + dy + 2.0f, 15.0f, MUI_C_FAINT);
}

static void DrawEntries(void)
{
    MUi_DrawPanel(kLeft);
    SectionTitle(kLeft, 12.0f, "Entradas", LIST_ENTRIES);

    const Rectangle r = EntryListRect();
    const int rows = MUi_ListRows(r, ENTRY_ROW_H);
    MUi_DrawListBegin(r);
    for (int i = entryList.top; i < draft.count && i <= entryList.top + rows; ++i) {
        const NotesplashEntry *e = &draft.entries[i];
        const Rectangle row = MUi_ListRow(&entryList, r, ENTRY_ROW_H, i);
        MUi_DrawListRow(row, i == entryList.sel);

        const char *base = strrchr(e->animset, '/');
        char line[96];
        snprintf(line, sizeof(line), "%d. %s", i + 1, base ? base + 1 : e->animset);
        MUi_Text(line, row.x + 10.0f, row.y + 4.0f, 18.0f, MUI_C_TEXT);
        MUi_Text(e->prefix, row.x + 26.0f, row.y + 24.0f, 15.0f, MUI_C_DIM);

        const Color tint = e->tint >= 0 ? EffectsConfig_TintColor(e->tint) : MUI_C_FAINT;
        DrawRectangleRounded((Rectangle){row.x + row.width - 18.0f, row.y + 8.0f, 10.0f, 10.0f}, 1.0f, 6, tint);
        if (i == draft.active) {
            const Rectangle badge = {row.x + row.width - 72.0f, row.y + 24.0f, 62.0f, 17.0f};
            DrawRectangleRounded(badge, 0.5f, 6, MUI_C_ACCENT);
            MUi_TextCenter("ATIVA", badge, 13.0f, WHITE);
        }
    }
    MUi_DrawListEnd(&entryList, r, ENTRY_ROW_H, draft.count);

    MUi_DrawButton(BtnCopy(), "+ Copia", MUI_BTN_NORMAL);
    MUi_DrawButton(BtnRemove(), "Remover", draft.count > 1 ? MUI_BTN_DANGER : MUI_BTN_OFF);
    const bool isActive = draft.active == entryList.sel;
    MUi_DrawButton(BtnActive(), isActive ? "Em uso na gameplay" : "Usar na gameplay",
                   isActive ? MUI_BTN_ACTIVE : MUI_BTN_ACCENT);
}

static void DrawAtlas(void)
{
    MUi_DrawPanel(kMid);
    SectionTitle(kMid, 12.0f, "Atlas", LIST_SHEETS);
    MUi_DrawTextField(SearchRect(), search, "buscar atlas...");

    const NotesplashEntry *e = Current();
    const Rectangle r = SheetListRect();
    const int rows = MUi_ListRows(r, LIST_ROW_H);
    MUi_DrawListBegin(r);
    for (int i = sheetList.top; i < visibleCount && i <= sheetList.top + rows; ++i) {
        const Sheet *s = &sheets[visible[i]];
        const Rectangle row = MUi_ListRow(&sheetList, r, LIST_ROW_H, i);
        const bool used = strcmp(s->path, e->animset) == 0;
        MUi_DrawListRow(row, used);
        MUi_Text(s->label, row.x + 10.0f, row.y + 4.0f, 17.0f, used ? WHITE : MUI_C_TEXT);
        MUi_TextRight(s->root, row.x + row.width - 8.0f, row.y + 5.0f, 14.0f, MUI_C_FAINT);
    }
    MUi_DrawListEnd(&sheetList, r, LIST_ROW_H, visibleCount);
    if (visibleCount == 0)
        MUi_Text(sheetCount ? "nada com esse nome" : "nenhum atlas: use Abrir (Ctrl+O)", r.x + 10.0f, r.y + 8.0f, 16.0f, MUI_C_FAINT);

    SectionTitle(kMid, 344.0f, "Animacao", LIST_ANIMS);
    const Rectangle a = AnimListRect();
    const int arows = MUi_ListRows(a, LIST_ROW_H);
    MUi_DrawListBegin(a);
    for (int i = animList.top; i < prefixCount && i <= animList.top + arows; ++i) {
        const Rectangle row = MUi_ListRow(&animList, a, LIST_ROW_H, i);
        const bool used = strcmp(prefixes[i], e->prefix) == 0;
        MUi_DrawListRow(row, used);
        MUi_Text(prefixes[i], row.x + 10.0f, row.y + 4.0f, 17.0f, used ? WHITE : MUI_C_TEXT);
    }
    MUi_DrawListEnd(&animList, a, LIST_ROW_H, prefixCount);
    if (prefixCount == 0)
        MUi_Text("atlas sem animacoes", a.x + 10.0f, a.y + 8.0f, 16.0f, MUI_C_FAINT);
}

static void DrawPreviewPanel(void)
{
    MUi_DrawPanel(kRight);
    SectionTitle(kRight, 12.0f, "Preview", -1);

    const Rectangle area = PreviewRect();
    DrawRectangleRounded(area, 0.03f, 6, (Color){6, 4, 14, 255});
    const float cx = area.x + area.width * 0.5f;
    const float cy = area.y + area.height * 0.5f;
    const Rectangle receptor = {cx - 56.0f, cy - 56.0f, 112.0f, 112.0f};
    DrawRectangleRounded(receptor, 0.25f, 8, (Color){40, 34, 70, 255});
    DrawRectangleRoundedLinesEx(receptor, 0.25f, 8, 2.0f, (Color){120, 100, 200, 180});

    MUi_ClipBegin(area);
    if (previewAnim >= 0)
        Render_DrawAnimatedObject(&previewObj);
    MUi_ClipEnd();
    if (previewAnim < 0)
        MUi_TextCenter("animacao nao encontrada no atlas", (Rectangle){area.x, area.y + area.height - 40.0f, area.width, 30.0f},
                       18.0f, MUI_C_BAD);

    const EffectsSplash *cfg = &EffectsConfig_Get()->splash;
    char info[128];
    snprintf(info, sizeof(info), "escala %d%%  opacidade %d%%  %d fps  (Effects Editor)", cfg->scalePct, cfg->alphaPct, cfg->fps);
    MUi_Text(info, area.x + 10.0f, area.y + 8.0f, 15.0f, MUI_C_FAINT);

    SectionTitle(kRight, 456.0f, "Cor", -1);
    const int tint = Current()->tint;
    for (int slot = 0; slot <= EFFECTS_TINTS; ++slot) {
        const Rectangle s = SwatchRect(slot);
        const Color c = EffectsConfig_TintColor(slot == 0 ? cfg->tint : slot - 1);
        MUi_DrawSwatch(s, c, tint == slot - 1);
        if (slot == 0)
            MUi_TextCenter("padrao", s, 14.0f, (Color){20, 16, 40, 255});
    }
    char line[96];
    snprintf(line, sizeof(line), "cor: %s", tint < 0 ? "padrao do Effects Editor" : EffectsConfig_TintName(tint));
    MUi_Text(line, kRight.x + 16.0f, kRight.y + 548.0f, 17.0f, MUI_C_DIM);
    MUi_Text(Current()->animset, kRight.x + 16.0f, kRight.y + 580.0f, 15.0f, MUI_C_FAINT);
}

static void Editor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    MUi_DrawTopBar("Notesplash Editor", "assets/notesplashes.json", dirty);
    MUi_DrawButton(kBtnOpen, "Abrir (Ctrl+O)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnSave, "Salvar", dirty ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);

    DrawEntries();
    DrawAtlas();
    DrawPreviewPanel();

    MUi_DrawBottomBar("Ctrl+O abre atlas por caminho   Ctrl+S salva   setas: lista ativa   Del remove entrada   Esc volta");
    MUi_DrawModal();
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void Editor_Create([[maybe_unused]] RayScene *scene)
{
    MUi_Reset();
    draft = *NotesplashConfig_Get();
    dirty = false;
    search[0] = '\0';
    prefixesOf[0] = '\0';
    prefixCount = 0;
    previewLoaded[0] = '\0';
    previewSet = (RayAnimationHandler){0};
    previewAnim = -1;
    previewT = 0.0f;
    entryList = sheetList = animList = (MUiList){0};
    activeList = LIST_ANIMS;

    ScanSheets();
    FilterSheets();
    SelectEntry(draft.active >= 0 && draft.active < draft.count ? draft.active : 0);
}

static void Editor_Destroy([[maybe_unused]] RayScene *scene)
{
    FreePreview();
    MUi_Reset();
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
