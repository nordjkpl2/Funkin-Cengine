/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Week Editor: escolhe uma semana (lista com busca, caminho digitado ou nova) e edita titulo,
 * descricao, fundo, personagens, flags e a lista de musicas (pasta, icone e cor), com preview da
 * capa e do icone. Salva <raiz>/weeks/<arquivo>.json preservando campos desconhecidos.
 *
 * Controles (edicao): clique nos campos e digite (Ctrl+C/V/X/A), Up/Down escolhe musica,
 * Ctrl+Up/Down muda a ordem, A adiciona, Del remove, Ctrl+S salva, Ctrl+O abre por caminho,
 * Esc volta a lista (pergunta se ha mudancas).
 */

#include "scenes/modding/weekeditor/WeekEditor.h"
#include "scenes/modding/weekeditor/WeekEditorIO.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/modding/ModdingUi.h"
#include "scenes/modding/ModdingMenu.h"
#include "characters/Character.h"
#include "Render.h"
#include "Game.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

#define FORM_ROW_H 46.0f
#define SONG_ROW_H 36.0f

enum { PROMPT_OPEN = 1, PROMPT_NEW, CONFIRM_LEAVE, CONFIRM_OPEN, CONFIRM_NEW };

typedef enum {
    WK_PICK = 0,
    WK_EDIT,
    WK_SONG
} WkMode;

static const Rectangle kPicker = {200.0f, 66.0f, 880.0f, 612.0f};
static const Rectangle kForm = {16.0f, 62.0f, 612.0f, 620.0f};
static const Rectangle kSongs = {640.0f, 62.0f, 624.0f, 620.0f};
static const Rectangle kBtnRight = {1104.0f, 8.0f, 160.0f, 32.0f};
static const Rectangle kBtnLeft = {934.0f, 8.0f, 160.0f, 32.0f};

/* campos de texto do formulario */
typedef enum {
    F_STORY = 0,
    F_NAME,
    F_FLAVOR,
    F_DIFFS,
    F_BG,
    F_BEFORE,
    F_TEXT_COUNT
} FormId;

static const char *const kFormLabel[F_TEXT_COUNT] = {
    "Title", "Short name", "Description", "Difficulties", "Background (cover)", "Previous week"
};
static const char *const kFormHint[F_TEXT_COUNT] = {
    "storyName (shown in Story Mode)", "weekName", "flavor (week text)",
    "empty = Easy, Normal, Hard", "menu_<name>.png in images/menubackgrounds", "empty = none"
};
static const MUiHelpRow kHelpPick[] = {
    {"type", "search week"},
    {"Enter / click", "open week"},
    {"Ctrl+O", "open by path"},
    {"Ctrl+N", "new week"},
    {"Esc", "return to editor menu"},
};
static const MUiHelpRow kHelpSong[] = {
    {"type", "search song folder"},
    {"Enter / click", "add song"},
    {"Esc", "cancel"},
};
static const MUiHelpRow kHelpEdit[] = {
    {"Click + type", "edit field (Ctrl+C / Ctrl+V)"},
    {"Up / Down", "select song"},
    {"Ctrl+Up / Down", "reorder song"},
    {"A", "add song"},
    {"Del", "remove song"},
    {"Ctrl+S", "save"},
    {"Ctrl+O", "open another week"},
    {"Esc", "back to week list"},
};

static const char *const kCharHint[3] = {"opponent", "player", "gf"};

/* ------------------------------------------------------------------------- */
/* Estado                                                                    */
/* ------------------------------------------------------------------------- */

static WkMode mode;
static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];

static WeekEdEntry entries[WEEKED_LIST_MAX];
static int entryCount;
static MUiPicker weekPicker;

static char songNames[WEEKED_LIST_MAX][MODED_NAME_MAX];
static int songNameCount;
static MUiPicker songPicker;

static WeekEdEntry current;
static WeekEdDraft draft;
static bool isNew;
static bool dirty;
static MUiList songList;

static Texture2D cover;
static char coverFor[64];
static bool coverFound;
static Texture2D icon;
static char iconFor[64];

/* ------------------------------------------------------------------------- */
/* Rotulos das listas                                                        */
/* ------------------------------------------------------------------------- */

static const char *WeekLabel(int i, int col)
{
    if (col == 0)
        return entries[i].title;
    if (col == 1)
        return entries[i].file;
    return ModEd_RootLabel(entries[i].root);
}

static const char *SongLabel(int i, int col)
{
    return col == 0 ? songNames[i] : NULL;
}

/* ------------------------------------------------------------------------- */
/* Preview (capa e icone)                                                    */
/* ------------------------------------------------------------------------- */

static void UnloadTex(Texture2D *t)
{
    if (t->id)
        UnloadTexture(*t);
    *t = (Texture2D){0};
}

/* Mesmos lugares do Story Mode: mod da semana, assets/mods e assets. */
static bool FindCover(const char *key, char *out, size_t cap)
{
    const char *bases[3] = {current.root, "assets/mods", "assets"};
    for (int i = 0; i < 3; ++i) {
        snprintf(out, cap, "%s/images/menubackgrounds/menu_%s.png", bases[i], key);
        if (FileExists(out))
            return true;
    }
    return false;
}

/* Mesma ordem do jogo: <nome>.png, depois icon-<nome>.png; mod e depois assets. */
static bool FindIcon(const char *name, char *out, size_t cap)
{
    const char *bases[2] = {current.root, "assets"};
    const char *fmts[2] = {"%s/images/icons/%s.png", "%s/images/icons/icon-%s.png"};
    for (int b = 0; b < 2; ++b) {
        for (int f = 0; f < 2; ++f) {
            snprintf(out, cap, fmts[f], bases[b], name);
            if (FileExists(out))
                return true;
        }
    }
    return false;
}

static void RefreshPreviews(void)
{
    if (strcmp(coverFor, draft.background) != 0) {
        snprintf(coverFor, sizeof(coverFor), "%s", draft.background);
        UnloadTex(&cover);
        char path[MODED_ROOT_MAX + 128];
        coverFound = draft.background[0] && ModEd_IsSafeName(draft.background) && FindCover(draft.background, path, sizeof(path));
        if (coverFound)
            cover = Render_LoadTexture(path);
    }

    const char *want = (draft.songCount > 0 && songList.sel < draft.songCount) ? draft.songs[songList.sel].icon : "";
    if (strcmp(iconFor, want) != 0) {
        snprintf(iconFor, sizeof(iconFor), "%s", want);
        UnloadTex(&icon);
        char path[MODED_ROOT_MAX + 128];
        if (want[0] && ModEd_IsSafeName(want) && FindIcon(want, path, sizeof(path)))
            icon = Render_LoadTexture(path);
    }
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar                                                            */
/* ------------------------------------------------------------------------- */

static void RescanWeeks(void)
{
    entryCount = WeekEd_ScanList(entries, WEEKED_LIST_MAX);
    MUi_PickerFilter(&weekPicker, entryCount, WeekLabel);
}

static void EnterEdit(void)
{
    mode = WK_EDIT;
    songList = (MUiList){0};
    coverFor[0] = '\1';   /* forca recarregar */
    iconFor[0] = '\1';
    MUi_TextBlur();
}

static bool OpenEntry(const WeekEdEntry *entry)
{
    char msg[256] = {0};
    WeekEdDraft loaded;
    if (!WeekEd_Load(entry, &loaded, msg, sizeof(msg))) {
        MUi_SetStatus(MUI_ERR, "%s", msg);
        return false;
    }
    current = *entry;
    draft = loaded;
    isNew = false;
    dirty = false;
    EnterEdit();
    MUi_SetStatus(MUI_OK, "aberto: %s/weeks/%s.json", current.root, current.file);
    return true;
}

static void CreateNew(const char *file)
{
    memset(&current, 0, sizeof(current));
    snprintf(current.file, sizeof(current.file), "%s", file);
    snprintf(current.root, sizeof(current.root), "%s", roots[0]);
    snprintf(current.title, sizeof(current.title), "%s", file);
    WeekEd_Defaults(&draft, file);
    isNew = true;
    dirty = true;
    EnterEdit();
    MUi_SetStatus(MUI_INFO, "new week in %s/weeks: Ctrl+S saves", current.root);
}

static bool SaveCurrent(void)
{
    char msg[300] = {0};
    if (!WeekEd_Save(&current, &draft, msg, sizeof(msg))) {
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
    mode = WK_PICK;
    dirty = false;
    RescanWeeks();
    MUi_TextBlur();
}

static void AskOpenPath(void)
{
    MUi_PromptOpen(PROMPT_OPEN, "Open week", "Path to the week .json. E.g.: assets/mods/MyMod/weeks/week1.json", "");
}

static void AskNew(const char *initial)
{
    char hint[MODED_ROOT_MAX + 96];
    snprintf(hint, sizeof(hint), "Nome do arquivo (letras, numeros, - _ .). Vai para %s/weeks/", roots[0]);
    MUi_PromptOpen(PROMPT_NEW, "New week", hint, initial);
}

static void OpenTypedPath(const char *typed)
{
    char path[MODED_ROOT_MAX * 2];
    snprintf(path, sizeof(path), "%s", typed);
    ModEd_CleanPath(path);

    /* so o nome: procura na lista */
    if (!strchr(path, '/')) {
        char *dot = strrchr(path, '.');
        if (dot && strcmp(dot, ".json") == 0)
            *dot = '\0';
        for (int i = 0; i < entryCount; ++i) {
            if (strcmp(entries[i].file, path) == 0) {
                OpenEntry(&entries[i]);
                return;
            }
        }
        MUi_PromptReopen("no week with that name: paste the .json path");
        return;
    }

    WeekEdEntry entry = {0};
    char msg[MODED_ROOT_MAX * 2 + 32];
    if (!ModEd_SplitPath(path, "weeks", entry.root, sizeof(entry.root), entry.file, sizeof(entry.file))) {
        MUi_PromptReopen("the file must be in a weeks/ folder (e.g.: .../weeks/week1.json)");
        return;
    }
    char full[MODED_ROOT_MAX + MODED_NAME_MAX + 16];
    snprintf(full, sizeof(full), "%s/weeks/%s.json", entry.root, entry.file);
    if (!FileExists(full)) {
        snprintf(msg, sizeof(msg), "not found: %s", full);
        MUi_PromptReopen(msg);
        return;
    }
    snprintf(entry.title, sizeof(entry.title), "%s", entry.file);
    if (!OpenEntry(&entry))
        MUi_PromptReopen("invalid JSON (see status bar)");
}

static void TryNew(const char *name)
{
    if (!ModEd_IsSafeName(name)) {
        MUi_PromptReopen("invalid name: use letters, digits, - _ . (no spaces)");
        return;
    }
    char path[MODED_ROOT_MAX + MODED_NAME_MAX + 16];
    snprintf(path, sizeof(path), "%s/weeks/%s.json", roots[0], name);
    if (FileExists(path)) {
        MUi_PromptReopen("a week with this name already exists");
        return;
    }
    CreateNew(name);
}

/* ------------------------------------------------------------------------- */
/* Musicas                                                                   */
/* ------------------------------------------------------------------------- */

static void MarkDirty(void)
{
    dirty = true;
}

static void SelectSong(int i)
{
    if (draft.songCount <= 0)
        return;
    songList.sel = i < 0 ? 0 : (i >= draft.songCount ? draft.songCount - 1 : i);
    MUi_TextBlur();
}

static void AddSong(const char *name)
{
    if (draft.songCount >= WEEK_MAX_SONGS) {
        MUi_SetStatus(MUI_WARN, "limit of %d songs", WEEK_MAX_SONGS);
        return;
    }
    WeekEdSong *song = &draft.songs[draft.songCount++];
    WeekEd_DefaultSong(song, name);
    if (draft.chars[0][0])
        snprintf(song->icon, sizeof(song->icon), "%s", draft.chars[0]);
    SelectSong(draft.songCount - 1);
    MarkDirty();
}

static void RemoveSong(void)
{
    const int s = songList.sel;
    if (s < 0 || s >= draft.songCount)
        return;
    for (int i = s; i < draft.songCount - 1; ++i)
        draft.songs[i] = draft.songs[i + 1];
    --draft.songCount;
    SelectSong(s);
    MarkDirty();
}

static void MoveSong(int delta)
{
    const int s = songList.sel, t = s + delta;
    if (s < 0 || t < 0 || s >= draft.songCount || t >= draft.songCount)
        return;
    const WeekEdSong tmp = draft.songs[s];
    draft.songs[s] = draft.songs[t];
    draft.songs[t] = tmp;
    SelectSong(t);
    MarkDirty();
}

static void OpenSongPicker(void)
{
    songNameCount = WeekEd_ScanSongs(songNames, WEEKED_LIST_MAX);
    songPicker.search[0] = '\0';
    songPicker.list = (MUiList){0};
    MUi_PickerFilter(&songPicker, songNameCount, SongLabel);
    MUi_TextBlur();
    mode = WK_SONG;
}

/* ------------------------------------------------------------------------- */
/* Retangulos da edicao                                                      */
/* ------------------------------------------------------------------------- */

static float FormRowY(int row) { return kForm.y + 44.0f + (float)row * FORM_ROW_H; }
static Rectangle FormField(int row) { return (Rectangle){kForm.x + 170.0f, FormRowY(row), kForm.width - 188.0f, 34.0f}; }
static Rectangle CharField(int i) { return (Rectangle){kForm.x + 170.0f + (float)i * 144.0f, FormRowY(F_TEXT_COUNT), 136.0f, 34.0f}; }
static Rectangle CheckRect(int i)
{
    const float y = FormRowY(F_TEXT_COUNT + 1);
    if (i == 0) return (Rectangle){kForm.x + 20.0f, y, 170.0f, 30.0f};
    if (i == 1) return (Rectangle){kForm.x + 196.0f, y, 200.0f, 30.0f};
    return (Rectangle){kForm.x + 400.0f, y, 200.0f, 30.0f};
}
static Rectangle CoverRect(void) { return (Rectangle){kForm.x + 20.0f, kForm.y + 476.0f, 236.0f, 132.0f}; }

static Rectangle SongListRect(void) { return (Rectangle){kSongs.x + 12.0f, kSongs.y + 42.0f, kSongs.width - 24.0f, 300.0f}; }
static Rectangle SongBtn(int i) { return (Rectangle){kSongs.x + 12.0f + (float)i * 152.0f, kSongs.y + 352.0f, 144.0f, 34.0f}; }
static Rectangle SongName(void) { return (Rectangle){kSongs.x + 100.0f, kSongs.y + 432.0f, 320.0f, 34.0f}; }
static Rectangle SongIcon(void) { return (Rectangle){kSongs.x + 100.0f, kSongs.y + 478.0f, 320.0f, 34.0f}; }
static Rectangle SongColor(int c) { return (Rectangle){kSongs.x + 100.0f + (float)c * 92.0f, kSongs.y + 524.0f, 84.0f, 34.0f}; }
static Rectangle IconBox(void) { return (Rectangle){kSongs.x + 450.0f, kSongs.y + 420.0f, 150.0f, 150.0f}; }

static char *FormText(int f)
{
    switch (f) {
        case F_STORY:  return draft.storyName;
        case F_NAME:   return draft.weekName;
        case F_FLAVOR: return draft.flavor;
        case F_DIFFS:  return draft.difficulties;
        case F_BG:     return draft.background;
        default:       return draft.before;
    }
}

static size_t FormCap(int f)
{
    switch (f) {
        case F_STORY:  return sizeof(draft.storyName);
        case F_NAME:   return sizeof(draft.weekName);
        case F_FLAVOR: return sizeof(draft.flavor);
        case F_DIFFS:  return sizeof(draft.difficulties);
        case F_BG:     return sizeof(draft.background);
        default:       return sizeof(draft.before);
    }
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
        case CONFIRM_NEW:
            if (res == MUI_MODAL_CANCEL || (res == MUI_MODAL_OK && !SaveCurrent()))
                break;
            dirty = false;
            if (id == CONFIRM_LEAVE)
                ToPick();
            else if (id == CONFIRM_OPEN)
                AskOpenPath();
            else
                AskNew("");
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

    const int pick = MUi_PickerUpdate(&weekPicker, kPicker, entryCount, WeekLabel);
    if (pick >= 0)
        OpenEntry(&entries[pick]);
    else if (pick == MUI_PICK_NEW)
        AskNew(weekPicker.search);
    else if (pick == MUI_PICK_BACK)
        ModdingMenu_SetScene();
}

static void Song_Update(void)
{
    if (MUi_Button(kBtnRight)) {
        mode = WK_EDIT;
        return;
    }
    const int pick = MUi_PickerUpdate(&songPicker, kPicker, songNameCount, SongLabel);
    if (pick >= 0) {
        AddSong(songNames[pick]);
        mode = WK_EDIT;
    } else if (pick == MUI_PICK_NEW) {
        if (ModEd_IsSafeName(songPicker.search)) {
            AddSong(songPicker.search);
            mode = WK_EDIT;
        } else {
            MUi_SetStatus(MUI_WARN, "invalid folder name");
        }
    } else if (pick == MUI_PICK_BACK) {
        mode = WK_EDIT;
    }
}

static void Edit_Update(void)
{
    if (MUi_Button(kBtnRight))
        SaveCurrent();
    if (MUi_Button(kBtnLeft)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_OPEN, "Open another week", "There are unsaved changes in this week.");
        else
            AskOpenPath();
        return;
    }

    /* formulario */
    for (int f = 0; f < F_TEXT_COUNT; ++f) {
        const MUiTextEvent ev = MUi_TextField(FormField(f), FormText(f), FormCap(f));
        if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
            MarkDirty();
    }
    for (int i = 0; i < 3; ++i) {
        const MUiTextEvent ev = MUi_TextField(CharField(i), draft.chars[i], sizeof(draft.chars[i]));
        if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
            MarkDirty();
    }
    bool *flags[3] = {&draft.startUnlocked, &draft.hideStoryMode, &draft.hideFreeplay};
    for (int i = 0; i < 3; ++i) {
        if (MUi_Check(CheckRect(i), flags[i]))
            MarkDirty();
    }

    /* musicas */
    const int clicked = MUi_List(&songList, SongListRect(), SONG_ROW_H, draft.songCount, NULL);
    if (clicked >= 0)
        SelectSong(clicked);
    if (MUi_Button(SongBtn(0))) {
        OpenSongPicker();
        return;
    }
    if (MUi_Button(SongBtn(1)))
        RemoveSong();
    if (MUi_Button(SongBtn(2)))
        MoveSong(-1);
    if (MUi_Button(SongBtn(3)))
        MoveSong(1);

    if (songList.sel >= 0 && songList.sel < draft.songCount) {
        WeekEdSong *song = &draft.songs[songList.sel];
        MUiTextEvent ev = MUi_TextField(SongName(), song->name, sizeof(song->name));
        if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
            MarkDirty();
        ev = MUi_TextField(SongIcon(), song->icon, sizeof(song->icon));
        if (ev == MUI_TXT_EDIT || ev == MUI_TXT_CANCEL)
            MarkDirty();
        for (int c = 0; c < 3; ++c) {
            if (MUi_NumInt(SongColor(c), &song->color[c], 1, 0, 255))
                MarkDirty();
        }
    }

    RefreshPreviews();

    if (!MUi_KeysFree())
        return;

    const bool ctrl = MUi_Ctrl();
    if (ctrl && IsKeyPressed(KEY_S)) {
        SaveCurrent();
        return;
    }
    if (ctrl && IsKeyPressed(KEY_O)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_OPEN, "Open another week", "There are unsaved changes in this week.");
        else
            AskOpenPath();
        return;
    }
    if (ctrl && IsKeyPressed(KEY_N)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_NEW, "New week", "There are unsaved changes in this week.");
        else
            AskNew("");
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty)
            MUi_ConfirmOpen(CONFIRM_LEAVE, "Back to list", "There are unsaved changes in this week.");
        else
            ToPick();
        return;
    }
    if (IsKeyPressed(KEY_A))
        OpenSongPicker();
    if (IsKeyPressed(KEY_DELETE))
        RemoveSong();
    if (ctrl && MUi_Rep(KEY_UP))
        MoveSong(-1);
    else if (ctrl && MUi_Rep(KEY_DOWN))
        MoveSong(1);
    else if (MUi_ListKeys(&songList, SongListRect(), SONG_ROW_H, draft.songCount))
        SelectSong(songList.sel);
}

static void WeekEditor_Update(RayScene *scene)
{
    MUi_FrameBegin();
    MUi_Tick(RayGame_DeltaTime());

    if (MUi_ModalActive())
        HandleModal();
    else if (MUi_HelpUpdate())
        ; /* overlay de atalhos aberto: o resto para */
    else if (mode == WK_PICK)
        Pick_Update();
    else if (mode == WK_SONG)
        Song_Update();
    else
        Edit_Update();

    if (MUi_Busy())
        RayScene_RequestRedraw(scene);
}

/* ------------------------------------------------------------------------- */
/* Draw                                                                      */
/* ------------------------------------------------------------------------- */

static void Pick_Draw(void)
{
    MUi_DrawTopBar("Week Editor", "choose a week", false);
    MUi_DrawButton(kBtnLeft, "Open (Ctrl+O)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnRight, "New (Ctrl+N)", MUI_BTN_ACCENT);
    MUi_DrawPicker(&weekPicker, kPicker, WeekLabel, "search week...",
                   entryCount ? "no week with that name: Enter creates a new one" : "no weeks: Enter creates a new one");
    MUi_DrawBottomBar();
}

static void Song_Draw(void)
{
    MUi_DrawTopBar("Week Editor", "add song (folder in songs/)", dirty);
    MUi_DrawButton(kBtnRight, "Cancel", MUI_BTN_NORMAL);
    MUi_DrawPicker(&songPicker, kPicker, SongLabel, "search song folder...",
                   "no folder with that name: Enter uses the typed name");
    MUi_DrawBottomBar();
}

static void DrawCover(void)
{
    const Rectangle box = CoverRect();
    DrawRectangleRounded(box, 0.04f, 6, (Color){6, 4, 14, 255});
    if (cover.id) {
        const float s = fminf(box.width / (float)cover.width, box.height / (float)cover.height);
        const float w = (float)cover.width * s, h = (float)cover.height * s;
        DrawTexturePro(cover, (Rectangle){0, 0, (float)cover.width, (float)cover.height},
                       (Rectangle){box.x + (box.width - w) * 0.5f, box.y + (box.height - h) * 0.5f, w, h},
                       (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        MUi_TextCenter("no cover", box, 17.0f, MUI_C_FAINT);
    }
    char line[160];
    const float tx = box.x + box.width + 14.0f;
    snprintf(line, sizeof(line), "menu_%s.png", draft.background[0] ? draft.background : "?");
    MUi_Text(line, tx, box.y + 4.0f, 16.0f, MUI_C_DIM);
    MUi_Text(coverFound ? "found" : "not found (uses default background)", tx, box.y + 26.0f, 15.0f,
             coverFound ? MUI_C_OK : MUI_C_HINT);
    snprintf(line, sizeof(line), "arquivo: %s/weeks/%s.json", current.root, current.file);
    MUi_ClipBegin((Rectangle){tx, box.y + 60.0f, kForm.x + kForm.width - tx - 10.0f, 60.0f});
    MUi_Text(line, tx, box.y + 60.0f, 15.0f, MUI_C_FAINT);
    MUi_Text(isNew ? "(not saved yet)" : "", tx, box.y + 80.0f, 15.0f, MUI_C_HINT);
    MUi_ClipEnd();
}

static void DrawForm(void)
{
    MUi_DrawPanel(kForm);
    MUi_DrawSection(kForm.x + 14.0f, kForm.y + 12.0f, "Week");

    for (int f = 0; f < F_TEXT_COUNT; ++f) {
        MUi_Text(kFormLabel[f], kForm.x + 20.0f, FormRowY(f) + 8.0f, 18.0f, MUI_C_DIM);
        MUi_DrawTextField(FormField(f), FormText(f), kFormHint[f]);
    }
    MUi_Text("Characters", kForm.x + 20.0f, FormRowY(F_TEXT_COUNT) + 8.0f, 18.0f, MUI_C_DIM);
    for (int i = 0; i < 3; ++i)
        MUi_DrawTextField(CharField(i), draft.chars[i], kCharHint[i]);

    MUi_DrawCheck(CheckRect(0), "Unlocked", draft.startUnlocked);
    MUi_DrawCheck(CheckRect(1), "Hide in Story", draft.hideStoryMode);
    MUi_DrawCheck(CheckRect(2), "Hide in Freeplay", draft.hideFreeplay);

    MUi_DrawSection(kForm.x + 14.0f, kForm.y + 448.0f, "Cover");
    DrawCover();
}

static void DrawSongs(void)
{
    MUi_DrawPanel(kSongs);
    char title[48];
    snprintf(title, sizeof(title), "Songs (%d/%d)", draft.songCount, WEEK_MAX_SONGS);
    MUi_DrawSection(kSongs.x + 14.0f, kSongs.y + 12.0f, title);

    const Rectangle r = SongListRect();
    const int rows = MUi_ListRows(r, SONG_ROW_H);
    MUi_DrawListBegin(r);
    for (int i = songList.top; i < draft.songCount && i <= songList.top + rows; ++i) {
        const WeekEdSong *song = &draft.songs[i];
        const Rectangle row = MUi_ListRow(&songList, r, SONG_ROW_H, i);
        MUi_DrawListRow(row, i == songList.sel);
        char num[8];
        snprintf(num, sizeof(num), "%d", i + 1);
        MUi_Text(num, row.x + 10.0f, row.y + 8.0f, 17.0f, MUI_C_FAINT);
        const Color c = {(unsigned char)song->color[0], (unsigned char)song->color[1], (unsigned char)song->color[2], 255};
        DrawRectangleRounded((Rectangle){row.x + 38.0f, row.y + 9.0f, 16.0f, 16.0f}, 0.4f, 4, c);
        MUi_Text(song->name[0] ? song->name : "(no name)", row.x + 64.0f, row.y + 6.0f, 19.0f,
                 song->name[0] ? MUI_C_TEXT : MUI_C_BAD);
        MUi_TextRight(song->icon, row.x + row.width - 10.0f, row.y + 9.0f, 15.0f, MUI_C_DIM);
    }
    MUi_DrawListEnd(&songList, r, SONG_ROW_H, draft.songCount);
    if (draft.songCount == 0)
        MUi_Text("no songs: + Song (A)", r.x + 12.0f, r.y + 10.0f, 18.0f, MUI_C_HINT);

    MUi_DrawButton(SongBtn(0), "+ Song (A)", MUI_BTN_NORMAL);
    MUi_DrawButton(SongBtn(1), "Remove", draft.songCount ? MUI_BTN_DANGER : MUI_BTN_OFF);
    MUi_DrawButton(SongBtn(2), "Up", songList.sel > 0 ? MUI_BTN_NORMAL : MUI_BTN_OFF);
    MUi_DrawButton(SongBtn(3), "Down", songList.sel + 1 < draft.songCount ? MUI_BTN_NORMAL : MUI_BTN_OFF);

    if (songList.sel < 0 || songList.sel >= draft.songCount)
        return;
    const WeekEdSong *song = &draft.songs[songList.sel];
    MUi_DrawSection(kSongs.x + 14.0f, kSongs.y + 400.0f, "Selected song");
    MUi_Text("Folder", kSongs.x + 20.0f, SongName().y + 8.0f, 18.0f, MUI_C_DIM);
    MUi_DrawTextField(SongName(), song->name, "folder name in songs/");
    MUi_Text("Icon", kSongs.x + 20.0f, SongIcon().y + 8.0f, 18.0f, MUI_C_DIM);
    MUi_DrawTextField(SongIcon(), song->icon, "images/icons/<nome>.png");
    MUi_Text("Color", kSongs.x + 20.0f, SongColor(0).y + 8.0f, 18.0f, MUI_C_DIM);
    for (int c = 0; c < 3; ++c)
        MUi_DrawNum(SongColor(c), (float)song->color[c], 1.0f);
    MUi_Text("R              G              B", kSongs.x + 132.0f, SongColor(0).y + 38.0f, 13.0f, MUI_C_FAINT);

    const Rectangle box = IconBox();
    const Color c = {(unsigned char)song->color[0], (unsigned char)song->color[1], (unsigned char)song->color[2], 255};
    DrawRectangleRounded(box, 0.1f, 6, c);
    if (icon.id) {
        const Rectangle src = Character_IconFrame(icon);
        DrawTexturePro(icon, src, box, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        MUi_TextCenter("no icon", box, 16.0f, (Color){20, 16, 40, 255});
    }
}

static void Edit_Draw(void)
{
    char detail[MODED_NAME_MAX + 48];
    snprintf(detail, sizeof(detail), "%s  [%s]", current.file, ModEd_RootLabel(current.root));
    MUi_DrawTopBar("Week Editor", detail, dirty);
    MUi_DrawButton(kBtnLeft, "Open (Ctrl+O)", MUI_BTN_NORMAL);
    MUi_DrawButton(kBtnRight, "Save (Ctrl+S)", dirty ? MUI_BTN_ACCENT : MUI_BTN_NORMAL);
    DrawForm();
    DrawSongs();
    MUi_DrawBottomBar();
}

static void WeekEditor_Draw([[maybe_unused]] RayScene *scene)
{
    MUi_DrawBackdrop();
    if (mode == WK_PICK)
        Pick_Draw();
    else if (mode == WK_SONG)
        Song_Draw();
    else
        Edit_Draw();
    if (mode == WK_PICK)
        MUi_DrawHelp("Week Editor - list", kHelpPick, (int)(sizeof(kHelpPick) / sizeof(kHelpPick[0])));
    else if (mode == WK_SONG)
        MUi_DrawHelp("Week Editor - add song", kHelpSong, (int)(sizeof(kHelpSong) / sizeof(kHelpSong[0])));
    else
        MUi_DrawHelp("Week Editor - shortcuts", kHelpEdit, (int)(sizeof(kHelpEdit) / sizeof(kHelpEdit[0])));
    MUi_DrawModal();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void WeekEditor_Create(RayScene *scene)
{
    scene->staticWhenIdle = true;   /* sem animacao: o loop pula o draw quando nada muda */
    MUi_Reset();
    ModEd_Roots(roots, MODED_ROOTS_MAX);
    memset(&draft, 0, sizeof(draft));
    memset(&current, 0, sizeof(current));
    memset(&weekPicker, 0, sizeof(weekPicker));
    memset(&songPicker, 0, sizeof(songPicker));
    isNew = false;
    dirty = false;
    cover = icon = (Texture2D){0};
    coverFor[0] = iconFor[0] = '\0';
    mode = WK_PICK;
    RescanWeeks();
}

static void WeekEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    UnloadTex(&cover);
    UnloadTex(&icon);
    MUi_Reset();
}

static RayScene weekEditorScene;
Scene_MakeSceneCode(
    weekEditorScene,
    WeekEditor_SetScene,
    WeekEditor_Create,
    WeekEditor_Update,
    WeekEditor_Draw,
    WeekEditor_Destroy
)
