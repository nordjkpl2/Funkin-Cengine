/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Week Editor: escolhe uma semana (ou digita um nome novo) e edita titulo, fundo, personagens,
 * flags e a lista de musicas (nome da pasta, icone e cor). Salva <raiz>/weeks/<arquivo>.json.
 *
 * Controles:
 *   Up/Down move | Enter edita campo texto (Enter confirma, ESC cancela) ou marca booleano
 *   A = adiciona musica (picker de songs/) | Del = remove a musica da linha | PgUp/PgDn = ordem
 *   Ctrl+S salva | ESC volta (2x se houver mudancas)
 */

#include "scenes/modding/weekeditor/WeekEditor.h"
#include "scenes/modding/weekeditor/WeekEditorIO.h"
#include "scenes/modding/ModdingCommon.h"
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/modding/ModdingMenu.h"
#include "Render.h"
#include "Game.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/* ------------------------------------------------------------------------- */

#define WK_FILTER_MAX 64
#define WK_ROWS       22
#define WK_ROW_H      25.0f
#define WK_LABEL_X    60.0f
#define WK_VALUE_X    440.0f
#define WK_TOP_Y      120.0f
#define WK_PICK_X     60.0f
#define WK_PICK_Y     140.0f
#define WK_PICK_ROW_H 26.0f
#define WK_PICK_ROWS  20

/* Linhas fixas do formulario; depois, 3 linhas por musica. */
#define WK_ROW_STORY   0
#define WK_ROW_NAME    1
#define WK_ROW_BG      2
#define WK_ROW_BEFORE  3
#define WK_ROW_CHAR0   4
#define WK_ROW_UNLOCK  7
#define WK_ROW_HIDE_ST 8
#define WK_ROW_HIDE_FP 9
#define WK_ROW_SONGS   10

typedef enum {
    WK_PICK = 0,
    WK_EDIT,
    WK_SONG
} WkMode;

static RayGraphicObject bg;
static Camera2D uiCam;
static WkMode mode;

static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];

/* escolha de semana */
static WeekEdEntry entries[WEEKED_LIST_MAX];
static int entryCount;
static char filter[WK_FILTER_MAX];
static int pickVisible[WEEKED_LIST_MAX];
static int pickVisibleCount;
static int pickSel;
static int pickTop;

/* semana aberta */
static WeekEdEntry current;
static bool isNew;
static WeekEdDraft draft;
static int row;
static int rowTop;
static bool editing;
static char editBackup[WEEKED_TEXT_MAX];
static bool dirty;
static bool escArmed;

/* picker de musicas */
static char songNames[WEEKED_LIST_MAX][MODED_NAME_MAX];
static int songCount;
static char songFilter[WK_FILTER_MAX];
static int songVisible[WEEKED_LIST_MAX];
static int songVisibleCount;
static int songSel;
static int songTop;

static char status[192];
static float statusTimer;

/* ------------------------------------------------------------------------- */
/* Texto, status, listas                                                     */
/* ------------------------------------------------------------------------- */

static void WK_Text(const char *text, float x, float y, float size, Color color)
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

static bool Pressed(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static void MoveIndex(int *index, int count, int delta)
{
    if (count <= 0)
        return;
    *index = (*index + delta + count) % count;
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

static void Rebuild_Pick(void)
{
    pickVisibleCount = 0;
    for (int i = 0; i < entryCount; ++i) {
        if (ContainsCI(entries[i].file, filter) || ContainsCI(entries[i].title, filter))
            pickVisible[pickVisibleCount++] = i;
    }
    if (pickSel >= pickVisibleCount)
        pickSel = pickVisibleCount > 0 ? pickVisibleCount - 1 : 0;
    pickTop = 0;
}

static void Rebuild_Songs(void)
{
    songVisibleCount = 0;
    for (int i = 0; i < songCount; ++i) {
        if (ContainsCI(songNames[i], songFilter))
            songVisible[songVisibleCount++] = i;
    }
    if (songSel >= songVisibleCount)
        songSel = songVisibleCount > 0 ? songVisibleCount - 1 : 0;
    songTop = 0;
}

/* ------------------------------------------------------------------------- */
/* Linhas do formulario                                                      */
/* ------------------------------------------------------------------------- */

static int RowCount(void)
{
    return WK_ROW_SONGS + 3 * draft.songCount;
}

static bool IsSongRow(int r)
{
    return r >= WK_ROW_SONGS;
}

static int SongOfRow(int r)
{
    return (r - WK_ROW_SONGS) / 3;
}

/* Campo de texto da linha (NULL para booleanos). cap recebe o tamanho do buffer. */
static char *RowText(int r, size_t *cap)
{
    if (IsSongRow(r)) {
        WeekEdSong *song = &draft.songs[SongOfRow(r)];
        switch ((r - WK_ROW_SONGS) % 3) {
            case 0: *cap = sizeof(song->name);  return song->name;
            case 1: *cap = sizeof(song->icon);  return song->icon;
            default: *cap = sizeof(song->color); return song->color;
        }
    }
    switch (r) {
        case WK_ROW_STORY:  *cap = sizeof(draft.storyName);  return draft.storyName;
        case WK_ROW_NAME:   *cap = sizeof(draft.weekName);   return draft.weekName;
        case WK_ROW_BG:     *cap = sizeof(draft.background); return draft.background;
        case WK_ROW_BEFORE: *cap = sizeof(draft.before);     return draft.before;
        case WK_ROW_CHAR0:
        case WK_ROW_CHAR0 + 1:
        case WK_ROW_CHAR0 + 2:
            *cap = sizeof(draft.chars[0]);
            return draft.chars[r - WK_ROW_CHAR0];
        default: *cap = 0; return NULL;
    }
}

static bool *RowBool(int r)
{
    switch (r) {
        case WK_ROW_UNLOCK:  return &draft.startUnlocked;
        case WK_ROW_HIDE_ST: return &draft.hideStoryMode;
        case WK_ROW_HIDE_FP: return &draft.hideFreeplay;
        default: return NULL;
    }
}

static void RowLabel(int r, char *out, size_t cap)
{
    if (IsSongRow(r)) {
        const int s = SongOfRow(r) + 1;
        switch ((r - WK_ROW_SONGS) % 3) {
            case 0: snprintf(out, cap, "musica %d: pasta em songs/", s); break;
            case 1: snprintf(out, cap, "musica %d: icone", s); break;
            default: snprintf(out, cap, "musica %d: cor r,g,b", s); break;
        }
        return;
    }
    switch (r) {
        case WK_ROW_STORY:   snprintf(out, cap, "titulo da historia (storyName)"); break;
        case WK_ROW_NAME:    snprintf(out, cap, "nome da semana (weekName)"); break;
        case WK_ROW_BG:      snprintf(out, cap, "fundo (weekBackground)"); break;
        case WK_ROW_BEFORE:  snprintf(out, cap, "semana anterior (weekBefore)"); break;
        case WK_ROW_CHAR0:   snprintf(out, cap, "personagem 1 (boyfriend/dad/gf)"); break;
        case WK_ROW_CHAR0+1: snprintf(out, cap, "personagem 2"); break;
        case WK_ROW_CHAR0+2: snprintf(out, cap, "personagem 3"); break;
        case WK_ROW_UNLOCK:  snprintf(out, cap, "desbloqueada (startUnlocked)"); break;
        case WK_ROW_HIDE_ST: snprintf(out, cap, "esconder no story mode"); break;
        case WK_ROW_HIDE_FP: snprintf(out, cap, "esconder no freeplay"); break;
        default:             out[0] = '\0'; break;
    }
}

static void MarkDirty(void)
{
    dirty = true;
    escArmed = false;
}

/* Remove a musica s e fecha o buco. */
static void RemoveSong(int s)
{
    if (s < 0 || s >= draft.songCount)
        return;
    for (int i = s; i < draft.songCount - 1; ++i)
        draft.songs[i] = draft.songs[i + 1];
    memset(&draft.songs[draft.songCount - 1], 0, sizeof(WeekEdSong));
    --draft.songCount;
    if (row >= RowCount())
        row = RowCount() - 1;
    MarkDirty();
}

static void MoveSong(int s, int delta)
{
    const int t = s + delta;
    if (s < 0 || t < 0 || s >= draft.songCount || t >= draft.songCount)
        return;
    WeekEdSong tmp = draft.songs[s];
    draft.songs[s] = draft.songs[t];
    draft.songs[t] = tmp;
    row = WK_ROW_SONGS + t * 3 + (row - WK_ROW_SONGS) % 3;
    MarkDirty();
}

static void AddSong(const char *name)
{
    if (draft.songCount >= WEEK_MAX_SONGS) {
        SetStatus("limite de musicas da semana atingido");
        return;
    }
    WeekEdSong *song = &draft.songs[draft.songCount++];
    memset(song, 0, sizeof(*song));
    snprintf(song->name, sizeof(song->name), "%s", name);
    snprintf(song->color, sizeof(song->color), "%s", "146,113,253");
    row = WK_ROW_SONGS + (draft.songCount - 1) * 3;
    MarkDirty();
}

/* ------------------------------------------------------------------------- */
/* Abrir / salvar                                                            */
/* ------------------------------------------------------------------------- */

static void OpenEntry(const WeekEdEntry *entry, bool create)
{
    current = *entry;
    isNew = create;

    if (create) {
        WeekEd_Defaults(&draft, entry->file);
        SetStatus("semana nova: salve com Ctrl+S");
    } else {
        char msg[200] = {0};
        if (!WeekEd_Load(&current, &draft, msg, sizeof(msg))) {
            SetStatus(msg);
            return;
        }
        SetStatus("aberto");
    }

    row = 0;
    rowTop = 0;
    editing = false;
    dirty = create;
    escArmed = false;
    mode = WK_EDIT;
}

static void SaveCurrent(void)
{
    char msg[256] = {0};
    const bool ok = WeekEd_Save(&current, &draft, msg, sizeof(msg));
    if (ok) {
        isNew = false;
        dirty = false;
        escArmed = false;
    }
    SetStatus(msg);
}

static void LeaveToPick(void)
{
    dirty = false;
    escArmed = false;
    editing = false;
    mode = WK_PICK;
    entryCount = WeekEd_ScanList(entries, WEEKED_LIST_MAX);
    Rebuild_Pick();
}

/* ------------------------------------------------------------------------- */
/* Update: escolha de semana                                                 */
/* ------------------------------------------------------------------------- */

static void Pick_Update(void)
{
    if (ModEd_TypeText(filter, sizeof(filter)))
        Rebuild_Pick();

    if (IsKeyPressed(KEY_ESCAPE)) {
        ModdingMenu_SetScene();
        return;
    }
    if (Pressed(KEY_DOWN))
        MoveIndex(&pickSel, pickVisibleCount, 1);
    if (Pressed(KEY_UP))
        MoveIndex(&pickSel, pickVisibleCount, -1);
    ScrollTo(&pickTop, pickSel, WK_PICK_ROWS);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 m = RayGame_MouseWorld();
        if (m.x >= WK_PICK_X && m.x < WK_PICK_X + 760.0f && m.y >= WK_PICK_Y) {
            const int idx = (int)((m.y - WK_PICK_Y) / WK_PICK_ROW_H) + pickTop;
            if (idx >= 0 && idx < pickVisibleCount)
                OpenEntry(&entries[pickVisible[idx]], false);
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (pickVisibleCount > 0) {
            OpenEntry(&entries[pickVisible[pickSel]], false);
        } else if (!ModEd_IsSafeName(filter)) {
            SetStatus("nome invalido: use letras, digitos, - _ . (sem espacos)");
        } else {
            WeekEdEntry fresh = {0};
            snprintf(fresh.file, sizeof(fresh.file), "%s", filter);
            snprintf(fresh.root, sizeof(fresh.root), "%s", roots[0]);
            snprintf(fresh.title, sizeof(fresh.title), "%s", filter);
            OpenEntry(&fresh, true);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Update: picker de musicas                                                 */
/* ------------------------------------------------------------------------- */

static void Song_Update(void)
{
    if (ModEd_TypeText(songFilter, sizeof(songFilter)))
        Rebuild_Songs();

    if (IsKeyPressed(KEY_ESCAPE)) {
        mode = WK_EDIT;
        return;
    }
    if (Pressed(KEY_DOWN))
        MoveIndex(&songSel, songVisibleCount, 1);
    if (Pressed(KEY_UP))
        MoveIndex(&songSel, songVisibleCount, -1);
    ScrollTo(&songTop, songSel, WK_PICK_ROWS);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (songVisibleCount > 0) {
            AddSong(songNames[songVisible[songSel]]);
            mode = WK_EDIT;
        } else if (ModEd_IsSafeName(songFilter)) {
            AddSong(songFilter);
            mode = WK_EDIT;
        } else {
            SetStatus("nome de musica invalido");
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Update: formulario                                                        */
/* ------------------------------------------------------------------------- */

static void StartEditing(int r)
{
    size_t cap = 0;
    char *text = RowText(r, &cap);
    if (!text)
        return;
    snprintf(editBackup, sizeof(editBackup), "%s", text);
    editing = true;
}

static void Edit_Update(void)
{
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (ctrl && IsKeyPressed(KEY_S)) {
        editing = false;
        SaveCurrent();
        return;
    }

    /* Edicao de texto: digita no campo; Enter confirma; ESC cancela. */
    if (editing) {
        size_t cap = 0;
        char *text = RowText(row, &cap);
        if (text && ModEd_TypeText(text, cap))
            MarkDirty();

        if (IsKeyPressed(KEY_ESCAPE)) {
            if (text)
                snprintf(text, cap, "%s", editBackup);
            editing = false;
            return;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_TAB)) {
            editing = false;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (dirty && !escArmed) {
            escArmed = true;
            SetStatus("ha mudancas nao salvas: ESC de novo sai sem salvar (Ctrl+S salva)");
            return;
        }
        LeaveToPick();
        return;
    }

    const int count = RowCount();
    if (Pressed(KEY_DOWN))
        MoveIndex(&row, count, 1);
    if (Pressed(KEY_UP))
        MoveIndex(&row, count, -1);
    ScrollTo(&rowTop, row, WK_ROWS);

    /* Mouse: clique seleciona a linha; clicar na linha ja selecionada age como Enter. */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 m = RayGame_MouseWorld();
        if (m.x >= WK_LABEL_X - 6.0f && m.x < 1220.0f && m.y >= WK_TOP_Y - 2.0f) {
            const int idx = (int)((m.y - WK_TOP_Y + 2.0f) / WK_ROW_H);
            const int target = rowTop + idx;
            if (idx >= 0 && idx < WK_ROWS && target < count) {
                if (target != row) {
                    row = target;
                } else {
                    bool *flag = RowBool(row);
                    if (flag) {
                        *flag = !*flag;
                        MarkDirty();
                    } else {
                        StartEditing(row);
                    }
                }
                return;
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        bool *flag = RowBool(row);
        if (flag) {
            *flag = !*flag;
            MarkDirty();
        } else {
            StartEditing(row);
        }
        return;
    }

    if (IsKeyPressed(KEY_A)) {
        songFilter[0] = '\0';
        songSel = 0;
        songCount = WeekEd_ScanSongs(songNames, WEEKED_LIST_MAX);
        Rebuild_Songs();
        mode = WK_SONG;
        return;
    }

    if (IsKeyPressed(KEY_DELETE) && IsSongRow(row))
        RemoveSong(SongOfRow(row));

    if (IsKeyPressed(KEY_PAGE_UP) && IsSongRow(row))
        MoveSong(SongOfRow(row), -1);
    if (IsKeyPressed(KEY_PAGE_DOWN) && IsSongRow(row))
        MoveSong(SongOfRow(row), 1);
}

static void WeekEditor_Update([[maybe_unused]] RayScene *scene)
{
    if (statusTimer > 0.0f)
        statusTimer -= RayGame_DeltaTime();

    if (mode == WK_PICK)
        Pick_Update();
    else if (mode == WK_SONG)
        Song_Update();
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

/* Chamado dentro de BeginUi(): coordenadas virtuais 1280x720. */
static void DrawBackdrop(void)
{
    ClearBackground(BLACK);
    Render_DrawGraphicObject(&bg);
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 150});
}

static void DrawStatus(void)
{
    if (statusTimer > 0.0f && status[0])
        WK_Text(status, 20.0f, 52.0f, 20.0f, YELLOW);
}

static void DrawFilterLine(const char *label, const char *text, float y)
{
    WK_Text(label, WK_PICK_X, y, 22.0f, GRAY);
    WK_Text(text, WK_PICK_X + 80.0f, y, 22.0f, WHITE);
    const float w = MeasureTextEx(mainFont, text, 22.0f, 1.0f).x;
    DrawRectangle((int)(WK_PICK_X + 80.0f + w + 2.0f), (int)y, 2, 22, WHITE);
}

static void Pick_Draw(void)
{
    WK_Text("WEEK EDITOR - escolha uma semana", 20.0f, 20.0f, 28.0f, WHITE);
    DrawFilterLine("filtro:", filter, 100.0f);

    for (int i = 0; i < WK_PICK_ROWS; ++i) {
        const int idx = pickTop + i;
        if (idx >= pickVisibleCount)
            break;
        const float y = WK_PICK_Y + i * WK_PICK_ROW_H;
        const bool on = idx == pickSel;
        if (on)
            DrawRectangle((int)WK_PICK_X - 6, (int)y - 2, 760, (int)WK_PICK_ROW_H, (Color){42, 209, 86, 60});
        const WeekEdEntry *e = &entries[pickVisible[idx]];
        char line[220];
        snprintf(line, sizeof(line), "%s   (%s)   [%s]", e->title, e->file, RootLabel(e->root));
        WK_Text(line, WK_PICK_X, y, 22.0f, on ? GREEN : WHITE);
    }

    if (entryCount == 0)
        WK_Text("nenhuma semana encontrada: digite um nome e Enter para criar", WK_PICK_X, WK_PICK_Y, 22.0f, GRAY);
    else if (pickVisibleCount == 0 && filter[0])
        WK_Text("Enter cria a semana nova com este nome", WK_PICK_X, WK_PICK_Y, 22.0f, GRAY);

    WK_Text("Up/Down escolhe   Enter abre   ESC volta ao menu", 20.0f, 690.0f, 18.0f, GRAY);
    DrawStatus();
}

static void Edit_Draw(void)
{
    char head[256];
    snprintf(head, sizeof(head), "WEEK EDITOR - %s [%s]%s%s", current.file, RootLabel(current.root),
             isNew ? " (nova)" : "", dirty ? " *nao salvo*" : "");
    WK_Text(head, 20.0f, 16.0f, 24.0f, WHITE);

    const int count = RowCount();
    for (int i = 0; i < WK_ROWS; ++i) {
        const int r = rowTop + i;
        if (r >= count)
            break;
        const float y = WK_TOP_Y + i * WK_ROW_H;
        const bool on = r == row;
        if (on)
            DrawRectangle((int)WK_LABEL_X - 6, (int)y - 2, 1160, (int)WK_ROW_H, (Color){42, 209, 86, 50});

        char label[96];
        RowLabel(r, label, sizeof(label));
        WK_Text(label, WK_LABEL_X, y, 19.0f, on ? GREEN : GRAY);

        bool *flag = RowBool(r);
        if (flag) {
            WK_Text(*flag ? "[x] sim" : "[ ] nao", WK_VALUE_X, y, 19.0f, WHITE);
            continue;
        }
        size_t cap = 0;
        const char *text = RowText(r, &cap);
        if (!text)
            continue;
        if (on && editing) {
            char shown[WEEKED_TEXT_MAX + 2];
            snprintf(shown, sizeof(shown), "%s_", text);
            WK_Text(shown, WK_VALUE_X, y, 19.0f, YELLOW);
        } else {
            WK_Text(text[0] ? text : "(vazio)", WK_VALUE_X, y, 19.0f, text[0] ? WHITE : DARKGRAY);
        }
    }

    char foot[160];
    snprintf(foot, sizeof(foot), "musicas: %d / %d", draft.songCount, WEEK_MAX_SONGS);
    WK_Text(foot, 20.0f, 650.0f, 18.0f, GRAY);
    DrawStatus();
    WK_Text(editing
                ? "digitando: Enter confirma   ESC cancela"
                : "Up/Down move   Enter edita/marca   A +musica   Del remove musica   PgUp/PgDn ordem   Ctrl+S salvar   ESC volta",
            20.0f, 690.0f, 16.0f, GRAY);
}

static void Song_Draw(void)
{
    WK_Text("adicionar musica (pasta em songs/)", WK_PICK_X, 100.0f - 30.0f, 22.0f, YELLOW);
    DrawFilterLine("filtro:", songFilter, 100.0f);

    for (int i = 0; i < WK_PICK_ROWS; ++i) {
        const int idx = songTop + i;
        if (idx >= songVisibleCount)
            break;
        const float y = WK_PICK_Y + i * WK_PICK_ROW_H;
        const bool on = idx == songSel;
        if (on)
            DrawRectangle((int)WK_PICK_X - 6, (int)y - 2, 500, (int)WK_PICK_ROW_H, (Color){42, 209, 86, 60});
        WK_Text(songNames[songVisible[idx]], WK_PICK_X, y, 22.0f, on ? GREEN : WHITE);
    }
    if (songVisibleCount == 0)
        WK_Text("nenhuma pasta em songs/: Enter usa o nome digitado", WK_PICK_X, WK_PICK_Y, 22.0f, GRAY);

    WK_Text("Up/Down escolhe   Enter adiciona   ESC cancela", 20.0f, 690.0f, 18.0f, GRAY);
}

static void WeekEditor_Draw([[maybe_unused]] RayScene *scene)
{
    BeginUi();
    DrawBackdrop();
    if (mode == WK_PICK)
        Pick_Draw();
    else if (mode == WK_SONG)
        Song_Draw();
    else
        Edit_Draw();
    EndUi();
}

/* ------------------------------------------------------------------------- */
/* Scene lifecycle                                                           */
/* ------------------------------------------------------------------------- */

static void WeekEditor_Create([[maybe_unused]] RayScene *scene)
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

    ModEd_Roots(roots, MODED_ROOTS_MAX);
    memset(&draft, 0, sizeof(draft));
    memset(&current, 0, sizeof(current));
    isNew = false;
    row = 0;
    rowTop = 0;
    editing = false;
    dirty = false;
    escArmed = false;

    mode = WK_PICK;
    filter[0] = '\0';
    pickSel = 0;
    pickTop = 0;
    entryCount = WeekEd_ScanList(entries, WEEKED_LIST_MAX);
    Rebuild_Pick();

    status[0] = '\0';
    statusTimer = 0.0f;
}

static void WeekEditor_Destroy([[maybe_unused]] RayScene *scene)
{
    bg.image = (Texture2D){0}; /* compartilhada: nao descarrega aqui */
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
