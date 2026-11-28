/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Dialogue Editor (DialogueEditorState da Psych): escolhe a musica (lista com busca ou caminho
 * digitado), edita as falas do dialogue.json com preview da caixa do jogo e salva.
 *
 * Controles (edicao): clique nos campos e digite, Up/Down ou A/D escolhe a fala, Ctrl+Up/Down
 * muda a ordem, P adiciona, O remove, W/S troca a expressao, Space digita de novo, Ctrl+S salva,
 * Ctrl+O abre por caminho, Esc volta a lista (pergunta se ha mudancas).
 * No texto, "\n" quebra a linha.
 */

#include "scenes/modding/dialogueeditor/DialogueEditor.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/modding/ModdingMenu.h"
#include "data/dialogue/Dialogue.h"
#include "logic/dialogue/DialogueBox.h"
#include "Render.h"
#include "Game.h"
#include "rlgl.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

#define DE_LIST_MAX 512
#define LINE_ROW_H 30.0f
#define PREVIEW_SCALE 0.6f
#define FIELD_X (kPanel.x + 118.0f)
#define FIELD_W (kPanel.width - 134.0f)
#define PATH_CAP (MODED_ROOT_MAX + MODED_NAME_MAX + 64)

enum { PROMPT_OPEN = 1, PROMPT_NEW, CONFIRM_LEAVE, CONFIRM_OPEN };

typedef enum { DE_PICK = 0, DE_EDIT } DeMode;

typedef struct {
    char root[MODED_ROOT_MAX];
    char song[MODED_NAME_MAX];
    bool has;
} DeEntry;

static const Rectangle kPicker = {200.0f, 66.0f, 880.0f, 612.0f};
static const Rectangle kPreview = {16.0f, 58.0f, 1280.0f * PREVIEW_SCALE, 720.0f * PREVIEW_SCALE};
static const Rectangle kLines = {16.0f, 500.0f, 1280.0f * PREVIEW_SCALE, 182.0f};
static const Rectangle kPanel = {796.0f, 58.0f, 468.0f, 624.0f};
static const Rectangle kBtnRight = {1104.0f, 8.0f, 160.0f, 32.0f};
static const Rectangle kBtnLeft = {934.0f, 8.0f, 160.0f, 32.0f};

static const MUiHelpRow kHelpPick[] = {
    {"type", "search song"},
    {"Enter / click", "open (or create) its dialogue.json"},
    {"Ctrl+O", "open by path"},
    {"Ctrl+N", "new dialogue for a song folder"},
    {"Esc", "return to editor menu"},
};
static const MUiHelpRow kHelpEdit[] = {
    {"Click + type", "edit field (\\n = new line in Text)"},
    {"Up / Down, A / D", "select line"},
    {"Ctrl+Up / Down", "reorder line"},
    {"P", "add line after the current one"},
    {"O / Del", "remove line"},
    {"W / S", "previous / next expression of the portrait"},
    {"Space", "type the line again"},
    {"Ctrl+S", "save"},
    {"Ctrl+O", "open another file"},
    {"Esc", "back to song list"},
};

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

static DeMode mode;
static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
static int rootCount;

static DeEntry entries[DE_LIST_MAX];
static int entryCount;
static MUiPicker picker;

static char filePath[PATH_CAP];
static char assetRoot[MODED_ROOT_MAX];
static char fileLabel[MODED_NAME_MAX];
static DialogueFile draft;
static bool dirty, isNew;
static MUiList lines;
static char textBuf[DIALOGUE_TEXT_MAX + 128];   /* texto com "\n" escrito */

static bool portraitFound, portraitHasDef;
static int portraitAnims;

/* ------------------------------------------------------------------------- */
/* Lista de musicas                                                          */
/* ------------------------------------------------------------------------- */

static const char *EntryLabel(int i, int col)
{
    if (col == 0)
        return entries[i].song;
    if (col == 1)
        return entries[i].has ? "dialogue.json" : NULL;
    return ModEd_RootLabel(entries[i].root);
}

/* <root>/songs/<musica>/dialogue.json, ou <root>/data/<musica>/dialogue.json (Psych). */
static bool FindDialogue(const char *root, const char *song, char *out, size_t cap)
{
    snprintf(out, cap, "%s/songs/%s/dialogue.json", root, song);
    if (FileExists(out))
        return true;
    char alt[PATH_CAP];
    snprintf(alt, sizeof(alt), "%s/data/%s/dialogue.json", root, song);
    if (FileExists(alt)) {
        snprintf(out, cap, "%s", alt);
        return true;
    }
    return false;
}

static void AddRoot(const char *root)
{
    for (int i = 0; i < rootCount; ++i) {
        if (strcmp(roots[i], root) == 0)
            return;
    }
    if (rootCount < MODED_ROOTS_MAX)
        snprintf(roots[rootCount++], MODED_ROOT_MAX, "%s", root);
}

/* Com dialogo primeiro; depois as outras musicas. */
static void Rescan(void)
{
    rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    if (DirectoryExists("assets/mods/songs"))
        AddRoot("assets/mods");   /* mod importado direto em assets/mods */

    entryCount = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int r = 0; r < rootCount; ++r) {
            char dir[MODED_ROOT_MAX + 16];
            snprintf(dir, sizeof(dir), "%s/songs", roots[r]);
            if (!DirectoryExists(dir))
                continue;
            FilePathList list = LoadDirectoryFilesEx(dir, "DIR", false);
            for (unsigned int i = 0; i < list.count && entryCount < DE_LIST_MAX; ++i) {
                const char *name = GetFileName(list.paths[i]);
                if (!ModEd_IsSafeName(name) || !DirectoryExists(list.paths[i]))
                    continue;
                char path[PATH_CAP];
                const bool has = FindDialogue(roots[r], name, path, sizeof(path));
                if (has != (pass == 0))
                    continue;
                DeEntry *e = &entries[entryCount++];
                snprintf(e->root, sizeof(e->root), "%s", roots[r]);
                snprintf(e->song, sizeof(e->song), "%s", name);
                e->has = has;
            }
            UnloadDirectoryFiles(list);
        }
    }
    MUi_PickerFilter(&picker, entryCount, EntryLabel);
}

/* ------------------------------------------------------------------------- */
/* Texto com "\n"                                                            */
/* ------------------------------------------------------------------------- */

static void Escape(const char *src, char *dst, size_t cap)
{
    size_t n = 0;
    for (; *src && n + 2 < cap; ++src) {
        if (*src == '\n') {
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *src;
        }
    }
    dst[n] = '\0';
}

static void Unescape(const char *src, char *dst, size_t cap)
{
    size_t n = 0;
    for (; *src && n + 1 < cap; ++src) {
        if (src[0] == '\\' && src[1] == 'n') {
            dst[n++] = '\n';
            ++src;
        } else {
            dst[n++] = *src;
        }
    }
    dst[n] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Fala atual e preview                                                      */
/* ------------------------------------------------------------------------- */

static DialogueLine *Cur(void)
{
    return (lines.sel >= 0 && lines.sel < draft.count) ? &draft.lines[lines.sel] : NULL;
}

static void Preview(bool instant)
{
    DialogueLine *line = Cur();
    if (!line)
        return;
    DialogueBox_ShowLine(line, instant);
    const DialogueCharDef *def = DialogueBox_Portrait(line->portrait, &portraitFound);
    portraitHasDef = def != NULL;
    portraitAnims = def ? def->animCount : 0;
}

static void MarkDirty(void)
{
    dirty = true;
}

static void SelectLine(int i)
{
    if (draft.count <= 0)
        return;
    lines.sel = i < 0 ? 0 : (i >= draft.count ? draft.count - 1 : i);
    MUi_ListShow(&lines, kLines, LINE_ROW_H, draft.count);
    MUi_TextBlur();
    Escape(draft.lines[lines.sel].text, textBuf, sizeof(textBuf));
    Preview(false);
}

static void AddLine(void)
{
    if (draft.count >= DIALOGUE_MAX_LINES) {
        MUi_SetStatus(MUI_WARN, "limit of %d lines", DIALOGUE_MAX_LINES);
        return;
    }
    const int at = draft.count > 0 ? lines.sel + 1 : 0;
    DialogueLine line;
    if (Cur()) {   /* mesmo retrato/caixa/som da fala atual: so muda o texto */
        line = *Cur();
        line.text[0] = '\0';
    } else {
        Dialogue_DefaultLine(&line);
    }
    for (int i = draft.count; i > at; --i)
        draft.lines[i] = draft.lines[i - 1];
    draft.lines[at] = line;
    ++draft.count;
    SelectLine(at);
    MarkDirty();
}

static void RemoveLine(void)
{
    const int s = lines.sel;
    if (s < 0 || s >= draft.count)
        return;
    for (int i = s; i < draft.count - 1; ++i)
        draft.lines[i] = draft.lines[i + 1];
    if (--draft.count <= 0) {   /* Psych: nunca fica vazio */
        Dialogue_DefaultLine(&draft.lines[0]);
        draft.count = 1;
    }
    SelectLine(s);
    MarkDirty();
}

static void MoveLine(int delta)
{
    const int s = lines.sel, t = s + delta;
    if (s < 0 || t < 0 || s >= draft.count || t >= draft.count)
        return;
    const DialogueLine tmp = draft.lines[s];
    draft.lines[s] = draft.lines[t];
    draft.lines[t] = tmp;
    SelectLine(t);
    MarkDirty();
}

/* W/S: proxima expressao entre as animacoes do retrato. */
static void CycleExpression(int delta)
{
    DialogueLine *line = Cur();
    if (!line)
        return;
    bool found = false;
    const DialogueCharDef *def = DialogueBox_Portrait(line->portrait, &found);
    if (!def || def->animCount <= 0) {
        MUi_SetStatus(MUI_WARN, "portrait \"%s\" has no animations", line->portrait);
        return;
    }
    int at = -1;
    for (int i = 0; i < def->animCount && at < 0; ++i) {
        if (strcmp(def->anims[i].anim, line->expression) == 0)
            at = i;
    }
    at = at < 0 ? 0 : (at + delta + def->animCount) % def->animCount;
    snprintf(line->expression, sizeof(line->expression), "%s", def->anims[at].anim);
    MarkDirty();
    Preview(true);
}

static void SetAngry(bool angry)
{
    DialogueLine *line = Cur();
    if (!line || (strcmp(line->boxState, "angry") == 0) == angry)
        return;
    snprintf(line->boxState, sizeof(line->boxState), "%s", angry ? "angry" : "normal");
    MarkDirty();
    Preview(true);
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar                                                            */
/* ------------------------------------------------------------------------- */

/* Raiz das imagens/sons: tudo antes de /songs/ ou /data/ (senao a pasta do arquivo). */
static void RootOfPath(const char *path, char *out, size_t cap)
{
    const char *markers[2] = {"/songs/", "/data/"};
    const char *best = NULL;
    for (int m = 0; m < 2; ++m) {
        for (const char *p = strstr(path, markers[m]); p; p = strstr(p + 1, markers[m]))
            best = (!best || p > best) ? p : best;
    }
    if (best) {
        snprintf(out, cap, "%.*s", (int)(best - path), path);
    } else {
        snprintf(out, cap, "%s", GetDirectoryPath(path));
    }
}

static void EnterEdit(void)
{
    mode = DE_EDIT;
    lines = (MUiList){0};
    MUi_TextBlur();
    DialogueBox_Begin(assetRoot);
    SelectLine(0);
}

/* path existente: le; senao comeca com uma fala padrao. */
static bool OpenFile(const char *path, const char *root, const char *label)
{
    snprintf(filePath, sizeof(filePath), "%s", path);
    snprintf(assetRoot, sizeof(assetRoot), "%s", root);
    snprintf(fileLabel, sizeof(fileLabel), "%s", label);
    isNew = !FileExists(path);
    if (isNew) {
        draft.count = 1;
        Dialogue_DefaultLine(&draft.lines[0]);
        dirty = true;
        MUi_SetStatus(MUI_INFO, "new dialogue: Ctrl+S saves %s", filePath);
    } else {
        if (!Dialogue_Load(path, &draft)) {
            MUi_SetStatus(MUI_ERR, "invalid dialogue JSON: %s", path);
            return false;
        }
        if (draft.count == 0) {
            draft.count = 1;
            Dialogue_DefaultLine(&draft.lines[0]);
        }
        dirty = false;
        MUi_SetStatus(MUI_OK, "opened: %s (%d lines)", path, draft.count);
    }
    EnterEdit();
    return true;
}

static void OpenEntry(const DeEntry *e)
{
    char path[PATH_CAP];
    if (!FindDialogue(e->root, e->song, path, sizeof(path)))
        snprintf(path, sizeof(path), "%s/songs/%s/dialogue.json", e->root, e->song);
    (void)OpenFile(path, e->root, e->song);
}

static bool SaveCurrent(void)
{
    char msg[PATH_CAP + 32] = {0};
    if (!Dialogue_Save(filePath, &draft, msg, sizeof(msg))) {
        MUi_SetStatus(MUI_ERR, "%s", msg);
        return false;
    }
    isNew = false;
    dirty = false;
    MUi_SetStatus(MUI_OK, "%s", msg);
    return true;
}

static void ToPick(void)
{
    DialogueBox_End();
    mode = DE_PICK;
    dirty = false;
    Rescan();
    MUi_TextBlur();
}

static void AskOpenPath(void)
{
    MUi_PromptOpen(PROMPT_OPEN, "Open dialogue",
                   "Path to the .json. E.g.: assets/mods/MyMod/songs/tutorial/dialogue.json", "");
}

static void AskNew(const char *initial)
{
    char hint[MODED_ROOT_MAX + 96];
    snprintf(hint, sizeof(hint), "Song folder name. Creates %s/songs/<name>/dialogue.json", roots[0]);
    MUi_PromptOpen(PROMPT_NEW, "New dialogue", hint, initial);
}

static void OpenTypedPath(const char *typed)
{
    char path[PATH_CAP];
    snprintf(path, sizeof(path), "%s", typed);
    ModEd_CleanPath(path);
    if (DirectoryExists(path)) {   /* pasta da musica */
        char full[PATH_CAP + 16];
        snprintf(full, sizeof(full), "%s/dialogue.json", path);
        snprintf(path, sizeof(path), "%s", full);
    }
    if (strcmp(ModEd_Ext(path), ".json") != 0) {
        MUi_PromptReopen("the file must be a .json");
        return;
    }
    if (!FileExists(path)) {
        char msg[PATH_CAP + 16];
        snprintf(msg, sizeof(msg), "not found: %s", path);
        MUi_PromptReopen(msg);
        return;
    }
    char root[MODED_ROOT_MAX];
    RootOfPath(path, root, sizeof(root));
    char label[MODED_NAME_MAX];
    snprintf(label, sizeof(label), "%s", GetFileName(GetDirectoryPath(path)));
    if (!OpenFile(path, root, label))
        MUi_PromptReopen("invalid JSON (see status bar)");
}

static void TryNew(const char *name)
{
    if (!ModEd_IsSafeName(name)) {
        MUi_PromptReopen("invalid name: use letters, digits, - _ . (no spaces)");
        return;
    }
    DeEntry e = {0};
    snprintf(e.root, sizeof(e.root), "%s", roots[0]);
    snprintf(e.song, sizeof(e.song), "%s", name);
    OpenEntry(&e);
}

/* ------------------------------------------------------------------------- */
/* Retangulos da edicao                                                      */
/* ------------------------------------------------------------------------- */

static float RowY(int row) { return kPanel.y + 46.0f + (float)row * 46.0f; }
static Rectangle Field(int row) { return (Rectangle){FIELD_X, RowY(row), FIELD_W, 34.0f}; }
static Rectangle ExprField(void) { return (Rectangle){FIELD_X, RowY(2), FIELD_W - 84.0f, 34.0f}; }
static Rectangle ExprBtn(int i) { return (Rectangle){FIELD_X + FIELD_W - 78.0f + (float)i * 42.0f, RowY(2), 36.0f, 34.0f}; }
static Rectangle BoxBtn(int i) { return (Rectangle){FIELD_X + (float)i * 120.0f, RowY(3), 112.0f, 34.0f}; }
static Rectangle SpeedField(void) { return (Rectangle){FIELD_X, RowY(4), 120.0f, 34.0f}; }
static Rectangle TextField(void) { return (Rectangle){kPanel.x + 16.0f, RowY(6) + 24.0f, kPanel.width - 32.0f, 34.0f}; }
static Rectangle LineBtn(int i) { return (Rectangle){kPanel.x + 16.0f + (float)i * 110.0f, RowY(8), 102.0f, 34.0f}; }
static Rectangle ReplayBtn(void) { return (Rectangle){kPanel.x + 16.0f, RowY(9), kPanel.width - 32.0f, 34.0f}; }

/* ------------------------------------------------------------------------- */
/* Update                                                                    */
/* ------------------------------------------------------------------------- */

static void HandleModal(void)
{
    int id = 0;
    const MUiModalResult res = MUi_ModalUpdate(&id);
    if (res == MUI_MODAL_NONE)
        return;

    switch (id) {
        case PROMPT_OPEN:
            if (res == MUI_MODAL_OK)
                OpenTypedPath(MUi_PromptText());
            break;
        case PROMPT_NEW:
            if (res == MUI_MODAL_OK)
                TryNew(MUi_PromptText());
            break;
        case CONFIRM_LEAVE:
        case CONFIRM_OPEN:
            if (res == MUI_MODAL_CANCEL || (res == MUI_MODAL_OK && !SaveCurrent()))
                break;
            dirty = false;
            if (id == CONFIRM_LEAVE)
                ToPick();
            else
                AskOpenPath();
            break;
        default:
            break;
    }
}

static void Pick_Update(void)
{
    if (MUi_Button(kBtnLeft) || (MUi_Ctrl() && IsKeyPressed(KEY_O))) {
        AskOpenPath();
        return;
    }
    if (MUi_Button(kBtnRight) || (MUi_Ctrl() && IsKeyPressed(KEY_N))) {
        AskNew("");
        return;
    }

    const int pick = MUi_PickerUpdate(&picker, kPicker, entryCount, EntryLabel);
    if (pick >= 0)
        OpenEntry(&entries[pick]);
    else if (pick == MUI_PICK_NEW)
        AskNew(picker.search);
    else if (pick == MUI_PICK_BACK)
        ModdingMenu_SetScene();
}

static void Edit_Fields(void)
{
    DialogueLine *line = Cur();
    if (!line)
        return;

    MUiTextEvent ev = MUi_TextField(Field(0), line->portrait, sizeof(line->portrait));
    if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
        MarkDirty();
    if (ev == MUI_TXT_SUBMIT || ev == MUI_TXT_BLUR || ev == MUI_TXT_CANCEL)
        Preview(true);   /* retrato so recarrega ao confirmar */

    ev = MUi_TextField(ExprField(), line->expression, sizeof(line->expression));
    if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL) {
        MarkDirty();
        Preview(true);
    }
    if (MUi_Button(ExprBtn(0)))
        CycleExpression(-1);
    if (MUi_Button(ExprBtn(1)))
        CycleExpression(1);

    if (MUi_Button(BoxBtn(0)))
        SetAngry(false);
    if (MUi_Button(BoxBtn(1)))
        SetAngry(true);

    if (MUi_Num(SpeedField(), &line->speed, 0.005f, 0.0f, 1.0f)) {
        MarkDirty();
        Preview(MUi_NumBusy());   /* arrastando: texto inteiro; passo unico: digita */
    }

    ev = MUi_TextField(Field(5), line->sound, sizeof(line->sound));
    if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
        MarkDirty();

    ev = MUi_TextField(TextField(), textBuf, sizeof(textBuf));
    if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL) {
        Unescape(textBuf, line->text, sizeof(line->text));
        MarkDirty();
        Preview(true);
    }
}

static void Edit_Update(void)
{
    if (MUi_Button(kBtnRight))
        SaveCurrent();
    if (MUi_Button(kBtnLeft)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_OPEN, "Open another dialogue", "There are unsaved changes in this dialogue.");
        else
            AskOpenPath();
        return;
    }

    Edit_Fields();

    const int before = lines.sel;
    const int clicked = MUi_List(&lines, kLines, LINE_ROW_H, draft.count, NULL);
    if (clicked >= 0 && clicked != before)
        SelectLine(clicked);

    if (MUi_Button(LineBtn(0)))
        AddLine();
    if (MUi_Button(LineBtn(1)))
        RemoveLine();
    if (MUi_Button(LineBtn(2)))
        MoveLine(-1);
    if (MUi_Button(LineBtn(3)))
        MoveLine(1);
    if (MUi_Button(ReplayBtn()))
        Preview(false);

    if (!MUi_KeysFree())
        return;

    const bool ctrl = MUi_Ctrl();
    if (ctrl && IsKeyPressed(KEY_S)) {
        SaveCurrent();
        return;
    }
    if (ctrl && IsKeyPressed(KEY_O)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_OPEN, "Open another dialogue", "There are unsaved changes in this dialogue.");
        else
            AskOpenPath();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_LEAVE, "Back to list", "There are unsaved changes in this dialogue.");
        else
            ToPick();
        return;
    }
    if (ctrl && MUi_Rep(KEY_UP)) {
        MoveLine(-1);
        return;
    }
    if (ctrl && MUi_Rep(KEY_DOWN)) {
        MoveLine(1);
        return;
    }
    if (ctrl)
        return;
    if (IsKeyPressed(KEY_P))
        AddLine();
    else if (IsKeyPressed(KEY_O) || IsKeyPressed(KEY_DELETE))
        RemoveLine();
    else if (IsKeyPressed(KEY_W))
        CycleExpression(-1);
    else if (IsKeyPressed(KEY_S))
        CycleExpression(1);
    else if (IsKeyPressed(KEY_SPACE))
        Preview(false);
    else if (MUi_Rep(KEY_A))
        SelectLine(lines.sel - 1);
    else if (MUi_Rep(KEY_D))
        SelectLine(lines.sel + 1);
    else if (MUi_ListKeys(&lines, kLines, LINE_ROW_H, draft.count))
        SelectLine(lines.sel);
}

static void DialogueEditor_Update(RayScene *scene)
{
    MUi_FrameBegin();
    MUi_Tick(RayGame_DeltaTime());

    if (MUi_ModalActive())
        HandleModal();
    else if (MUi_HelpUpdate())
        ; /* overlay de atalhos aberto: o resto para */
    else if (mode == DE_PICK)
        Pick_Update();
    else
        Edit_Update();

    if (mode == DE_EDIT)
        (void)DialogueBox_Update(RayGame_DeltaTime(), false, false);

    scene->staticWhenIdle = mode == DE_PICK;   /* o preview anima o tempo todo */
    if (MUi_Busy())
        RayScene_RequestRedraw(scene);
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

static void Pick_Draw(void)
{
    MUi_DrawTopBar("Dialogue Editor", "choose a song", false);
    MUi_DrawButton(kBtnLeft, "Open (Ctrl+O)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnRight, "New (Ctrl+N)", MUI_BTN_ACCENT);
    MUi_DrawPicker(&picker, kPicker, EntryLabel, "search song...",
                   entryCount ? "no song with that name: Enter creates a new dialogue"
                              : "no songs: Enter creates a new dialogue");
    MUi_DrawBottomBar();
}

static void DrawPreview(void)
{
    DrawRectangleRec(kPreview, (Color){128, 128, 128, 255});   /* fundo cinza do editor da Psych */
    MUi_ClipBegin(kPreview);
    rlPushMatrix();
    rlTranslatef(kPreview.x, kPreview.y, 0.0f);
    rlScalef(PREVIEW_SCALE, PREVIEW_SCALE, 1.0f);
    DialogueBox_Draw();
    rlPopMatrix();
    MUi_ClipEnd();
    DrawRectangleLinesEx(kPreview, 1.0f, MUI_C_EDGE);
}

static void DrawLines(void)
{
    const int rows = MUi_ListRows(kLines, LINE_ROW_H);
    MUi_DrawListBegin(kLines);
    for (int i = lines.top; i < draft.count && i <= lines.top + rows; ++i) {
        const DialogueLine *line = &draft.lines[i];
        const Rectangle row = MUi_ListRow(&lines, kLines, LINE_ROW_H, i);
        MUi_DrawListRow(row, i == lines.sel);
        char buf[DIALOGUE_TEXT_MAX];
        snprintf(buf, sizeof(buf), "%d", i + 1);
        MUi_Text(buf, row.x + 8.0f, row.y + 7.0f, 15.0f, MUI_C_FAINT);
        MUi_Text(line->portrait, row.x + 44.0f, row.y + 6.0f, 16.0f,
                 strcmp(line->boxState, "angry") == 0 ? MUI_C_BAD : MUI_C_DIM);
        snprintf(buf, sizeof(buf), "%s", line->text[0] ? line->text : "(empty)");
        for (char *c = buf; *c; ++c) {
            if (*c == '\n')
                *c = ' ';
        }
        MUi_ClipBegin((Rectangle){row.x + 170.0f, row.y, row.width - 180.0f, row.height});
        MUi_Text(buf, row.x + 170.0f, row.y + 6.0f, 16.0f, line->text[0] ? MUI_C_TEXT : MUI_C_FAINT);
        MUi_ClipEnd();
    }
    MUi_DrawListEnd(&lines, kLines, LINE_ROW_H, draft.count);
}

static void DrawPanel(void)
{
    const DialogueLine *line = Cur();
    MUi_DrawPanel(kPanel);
    char title[48];
    snprintf(title, sizeof(title), "Line %d / %d", lines.sel + 1, draft.count);
    MUi_DrawSection(kPanel.x + 14.0f, kPanel.y + 12.0f, title);
    if (!line)
        return;

    const float lx = kPanel.x + 16.0f;
    MUi_Text("Portrait", lx, RowY(0) + 8.0f, 17.0f, MUI_C_DIM);
    MUi_DrawTextField(Field(0), line->portrait, "images/dialogue/<name>.json");
    char info[160];
    if (portraitFound)
        snprintf(info, sizeof(info), "found, %d animations", portraitAnims);
    else if (portraitHasDef)
        snprintf(info, sizeof(info), "json not found: using \"%s\"", DIALOGUE_DEFAULT_CHAR);
    else
        snprintf(info, sizeof(info), "json not found (no portrait)");
    MUi_Text(info, FIELD_X + 4.0f, RowY(1) + 4.0f, 15.0f, portraitFound ? MUI_C_OK : MUI_C_HINT);

    MUi_Text("Expression", lx, RowY(2) + 8.0f, 17.0f, MUI_C_DIM);
    MUi_DrawTextField(ExprField(), line->expression, "empty = any animation");
    MUi_DrawButton(ExprBtn(0), "<", portraitAnims ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(ExprBtn(1), ">", portraitAnims ? MUI_BTN_NORMAL : MUI_BTN_OFF);

    const bool angry = strcmp(line->boxState, "angry") == 0;
    MUi_Text("Box", lx, RowY(3) + 8.0f, 17.0f, MUI_C_DIM);
    MUi_DrawButton(BoxBtn(0), "Normal", angry ? MUI_BTN_NORMAL : MUI_BTN_ACTIVE);
    MUi_DrawButton(BoxBtn(1), "Angry", angry ? MUI_BTN_ACTIVE : MUI_BTN_NORMAL);

    MUi_Text("Speed", lx, RowY(4) + 8.0f, 17.0f, MUI_C_DIM);
    MUi_DrawNum(SpeedField(), line->speed, 0.005f);
    MUi_Text("seconds per letter", SpeedField().x + SpeedField().width + 12.0f, RowY(4) + 9.0f, 15.0f, MUI_C_FAINT);

    MUi_Text("Sound", lx, RowY(5) + 8.0f, 17.0f, MUI_C_DIM);
    MUi_DrawTextField(Field(5), line->sound, "sounds/<name>.ogg (empty = dialogue)");

    MUi_Text("Text", lx, RowY(6) + 2.0f, 17.0f, MUI_C_DIM);
    MUi_Text("\\n = new line", kPanel.x + kPanel.width - 130.0f, RowY(6) + 3.0f, 14.0f, MUI_C_FAINT);
    MUi_DrawTextField(TextField(), textBuf, "what the character says");

    MUi_DrawButton(LineBtn(0), "+ Line (P)", MUI_BTN_NORMAL);
    MUi_DrawButton(LineBtn(1), "Remove (O)", draft.count > 1 ? MUI_BTN_DANGER : MUI_BTN_OFF);
    MUi_DrawButton(LineBtn(2), "Up", lines.sel > 0 ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(LineBtn(3), "Down", lines.sel + 1 < draft.count ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(ReplayBtn(), DialogueBox_TextDone() ? "Replay (Space)" : "Typing...", MUI_BTN_NORMAL);

    const float iy = RowY(10) + 10.0f;
    MUi_DrawSection(kPanel.x + 14.0f, iy, "Files");
    MUi_ClipBegin((Rectangle){lx, iy + 30.0f, kPanel.width - 32.0f, 90.0f});
    MUi_Text(filePath, lx, iy + 32.0f, 15.0f, MUI_C_FAINT);
    MUi_Text(isNew ? "(not saved yet)" : "", lx, iy + 52.0f, 15.0f, MUI_C_HINT);
    MUi_Text(DialogueBox_HasBubble() ? "speech bubble: images/speech_bubble.png"
                                     : "speech bubble: drawn (no images/speech_bubble.png/.xml)",
             lx, iy + 72.0f, 15.0f, DialogueBox_HasBubble() ? MUI_C_OK : MUI_C_HINT);
    MUi_ClipEnd();
}

static void Edit_Draw(void)
{
    char detail[MODED_NAME_MAX + MODED_ROOT_MAX + 8];
    snprintf(detail, sizeof(detail), "%s  [%s]", fileLabel, ModEd_RootLabel(assetRoot));
    MUi_DrawTopBar("Dialogue Editor", detail, dirty);
    MUi_DrawButton(kBtnLeft, "Open (Ctrl+O)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnRight, "Save (Ctrl+S)", dirty ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);
    DrawPreview();
    DrawLines();
    DrawPanel();
    MUi_DrawBottomBar();
}

static void DialogueEditor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    if (mode == DE_PICK) {
        Pick_Draw();
        MUi_DrawHelp("Dialogue Editor - list", kHelpPick, (int)(sizeof(kHelpPick) / sizeof(kHelpPick[0])));
    } else {
        Edit_Draw();
        MUi_DrawHelp("Dialogue Editor - shortcuts", kHelpEdit, (int)(sizeof(kHelpEdit) / sizeof(kHelpEdit[0])));
    }
    MUi_DrawModal();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void DialogueEditor_Create(RayScene *scene)
{
    scene->staticWhenIdle = true;
    MUi_Reset();
    memset(&picker, 0, sizeof(picker));
    draft.count = 0;
    dirty = isNew = false;
    filePath[0] = assetRoot[0] = fileLabel[0] = '\0';
    mode = DE_PICK;
    Rescan();
}

static void DialogueEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    DialogueBox_End();
    MUi_Reset();
}

static RayScene dialogueEditorScene;
Scene_MakeSceneCode(
    dialogueEditorScene,
    DialogueEditor_SetScene,
    DialogueEditor_Create,
    DialogueEditor_Update,
    DialogueEditor_Draw,
    DialogueEditor_Destroy
)
