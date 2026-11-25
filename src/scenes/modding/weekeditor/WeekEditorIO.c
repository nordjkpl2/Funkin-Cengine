/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Week Editor: IO com yyjson. Formato Psych: songs = [["nome", "icone", [r,g,b]], ...]. */
#include "scenes/modding/weekeditor/WeekEditorIO.h"

#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "yyjson.h"


static bool Entry_Known(const WeekEdEntry *out, int count, const char *file)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(out[i].file, file) == 0)
            return true;
    }
    return false;
}

int WeekEd_ScanList(WeekEdEntry *out, int capacity)
{
    static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
    static char names[WEEKED_LIST_MAX][MODED_NAME_MAX];
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    const int rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    for (int r = 0; r < rootCount; ++r) {
        char dir[MODED_ROOT_MAX + 16];
        snprintf(dir, sizeof(dir), "%s/weeks", roots[r]);

        const int n = ModEd_ListStems(dir, ".json", names, WEEKED_LIST_MAX);
        for (int i = 0; i < n && count < capacity; ++i) {
            if (!ModEd_IsSafeName(names[i]) || Entry_Known(out, count, names[i]))
                continue;

            WeekEdEntry *entry = &out[count];
            snprintf(entry->file, sizeof(entry->file), "%s", names[i]);
            snprintf(entry->root, sizeof(entry->root), "%s", roots[r]);

            /* Titulo para a lista: storyName, senao weekName, senao o nome do arquivo. */
            WeekEdDraft peek;
            entry->title[0] = '\0';
            if (WeekEd_Load(entry, &peek, NULL, 0))
                snprintf(entry->title, sizeof(entry->title), "%s", peek.storyName[0] ? peek.storyName : peek.weekName);
            if (!entry->title[0])
                snprintf(entry->title, sizeof(entry->title), "%s", entry->file);
            ++count;
        }
    }
    return count;
}

int WeekEd_ScanSongs(char out[][MODED_NAME_MAX], int capacity)
{
    static char roots[MODED_ROOTS_MAX][MODED_ROOT_MAX];
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    const int rootCount = ModEd_Roots(roots, MODED_ROOTS_MAX);
    for (int r = 0; r < rootCount; ++r) {
        char dir[MODED_ROOT_MAX + 16];
        snprintf(dir, sizeof(dir), "%s/songs", roots[r]);
        if (!DirectoryExists(dir))
            continue;

        FilePathList list = LoadDirectoryFilesEx(dir, "DIR", false);
        for (unsigned int i = 0; i < list.count && count < capacity; ++i) {
            const char *name = GetFileName(list.paths[i]);
            if (!ModEd_IsSafeName(name) || !DirectoryExists(list.paths[i]))
                continue;

            bool known = false;
            for (int k = 0; k < count; ++k) {
                if (strcmp(out[k], name) == 0) {
                    known = true;
                    break;
                }
            }
            if (!known)
                snprintf(out[count++], MODED_NAME_MAX, "%s", name);
        }
        UnloadDirectoryFiles(list);
    }
    return count;
}

void WeekEd_Defaults(WeekEdDraft *draft, const char *title)
{
    memset(draft, 0, sizeof(*draft));
    snprintf(draft->storyName, sizeof(draft->storyName), "%s", title ? title : "");
    snprintf(draft->weekName, sizeof(draft->weekName), "%s", title ? title : "");
    snprintf(draft->background, sizeof(draft->background), "%s", "stage");
    snprintf(draft->chars[0], sizeof(draft->chars[0]), "%s", "dad");
    snprintf(draft->chars[1], sizeof(draft->chars[1]), "%s", "bf");
    snprintf(draft->chars[2], sizeof(draft->chars[2]), "%s", "gf");
    draft->startUnlocked = true;
}

void WeekEd_DefaultSong(WeekEdSong *song, const char *name)
{
    memset(song, 0, sizeof(*song));
    snprintf(song->name, sizeof(song->name), "%s", name ? name : "");
    snprintf(song->icon, sizeof(song->icon), "%s", "dad");
    song->color[0] = 146;
    song->color[1] = 113;
    song->color[2] = 253;
}

/* ------------------------------------------------------------------------- */
/* Leitura                                                                   */
/* ------------------------------------------------------------------------- */

static double Num(yyjson_val *v)
{
    if (yyjson_is_real(v)) return yyjson_get_real(v);
    if (yyjson_is_sint(v)) return (double)yyjson_get_sint(v);
    if (yyjson_is_uint(v)) return (double)yyjson_get_uint(v);
    return 0.0;
}

static void CopyStr(char *dst, size_t cap, yyjson_val *v)
{
    const char *s = yyjson_is_str(v) ? yyjson_get_str(v) : "";
    snprintf(dst, cap, "%s", s);
}

static int Clamp255(double v)
{
    return v < 0.0 ? 0 : (v > 255.0 ? 255 : (int)v);
}

static void CopyColor(int dst[3], yyjson_val *v)
{
    if (!yyjson_is_arr(v) || yyjson_arr_size(v) < 3)
        return;
    for (size_t i = 0; i < 3; ++i)
        dst[i] = Clamp255(Num(yyjson_arr_get(v, i)));
}

static bool ReadBool(yyjson_val *obj, const char *key, bool fallback)
{
    yyjson_val *v = yyjson_obj_get(obj, key);
    return yyjson_is_bool(v) ? yyjson_get_bool(v) : fallback;
}

bool WeekEd_Load(const WeekEdEntry *entry, WeekEdDraft *draft, char *msg, size_t msgCap)
{
    char path[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    snprintf(path, sizeof(path), "%s/weeks/%s.json", entry->root, entry->file);

    yyjson_doc *doc = yyjson_read_file(path, 0, NULL, NULL);
    if (!doc) {
        if (msg && msgCap)
            snprintf(msg, msgCap, "nao consegui ler: %s", path);
        return false;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        if (msg && msgCap)
            snprintf(msg, msgCap, "JSON nao e um objeto: %s", path);
        return false;
    }

    WeekEd_Defaults(draft, entry->file);
    CopyStr(draft->storyName, sizeof(draft->storyName), yyjson_obj_get(root, "storyName"));
    CopyStr(draft->weekName, sizeof(draft->weekName), yyjson_obj_get(root, "weekName"));
    yyjson_val *flavor = yyjson_obj_get(root, "flavor");
    CopyStr(draft->flavor, sizeof(draft->flavor), yyjson_is_str(flavor) ? flavor : yyjson_obj_get(root, "description"));
    CopyStr(draft->difficulties, sizeof(draft->difficulties), yyjson_obj_get(root, "difficulties"));
    CopyStr(draft->background, sizeof(draft->background), yyjson_obj_get(root, "weekBackground"));
    CopyStr(draft->before, sizeof(draft->before), yyjson_obj_get(root, "weekBefore"));
    draft->startUnlocked = ReadBool(root, "startUnlocked", true);
    draft->hideStoryMode = ReadBool(root, "hideStoryMode", false);
    draft->hideFreeplay = ReadBool(root, "hideFreeplay", false);

    yyjson_val *chars = yyjson_obj_get(root, "weekCharacters");
    for (int i = 0; i < 3; ++i) {
        draft->chars[i][0] = '\0';
        if (yyjson_is_arr(chars) && (size_t)i < yyjson_arr_size(chars))
            CopyStr(draft->chars[i], sizeof(draft->chars[i]), yyjson_arr_get(chars, (size_t)i));
    }

    yyjson_val *songs = yyjson_obj_get(root, "songs");
    if (yyjson_is_arr(songs)) {
        size_t idx, max;
        yyjson_val *item;
        yyjson_arr_foreach(songs, idx, max, item) {
            if (draft->songCount >= WEEK_MAX_SONGS || !yyjson_is_arr(item) || yyjson_arr_size(item) < 1)
                continue;
            WeekEdSong *song = &draft->songs[draft->songCount++];
            WeekEd_DefaultSong(song, NULL);
            CopyStr(song->name, sizeof(song->name), yyjson_arr_get(item, 0));
            CopyStr(song->icon, sizeof(song->icon), yyjson_arr_get(item, 1));
            CopyColor(song->color, yyjson_arr_get(item, 2));
        }
    }

    yyjson_doc_free(doc);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Escrita                                                                   */
/* ------------------------------------------------------------------------- */

bool WeekEd_Save(const WeekEdEntry *entry, const WeekEdDraft *draft, char *msg, size_t msgCap)
{
    char dir[MODED_ROOT_MAX + 16];
    char path[MODED_ROOT_MAX + MODED_NAME_MAX + 32];
    snprintf(dir, sizeof(dir), "%s/weeks", entry->root);
    snprintf(path, sizeof(path), "%s/%s.json", dir, entry->file);
    MakeDirectory(dir);

    /* Se existe mas nao le, nao sobrescreve: perderia o que o autor escreveu. */
    yyjson_mut_doc *doc = NULL;
    if (FileExists(path)) {
        yyjson_doc *source = yyjson_read_file(path, 0, NULL, NULL);
        if (!source) {
            snprintf(msg, msgCap, "JSON invalido, nao salvei: %s", path);
            return false;
        }
        doc = yyjson_doc_mut_copy(source, NULL);
        yyjson_doc_free(source);
    }
    if (!doc)
        doc = yyjson_mut_doc_new(NULL);
    if (!doc) {
        snprintf(msg, msgCap, "sem memoria para salvar");
        return false;
    }

    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_obj(root)) {
        root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
    }

    ModEd_PutStr(doc, root, "storyName", draft->storyName);
    ModEd_PutStr(doc, root, "weekName", draft->weekName);
    /* descricao: fica na chave que o arquivo ja usa (Psych "flavor"; alguns mods "description") */
    const bool usesDescription = yyjson_mut_obj_get(root, "description") && !yyjson_mut_obj_get(root, "flavor");
    ModEd_PutStr(doc, root, usesDescription ? "description" : "flavor", draft->flavor);
    ModEd_PutStr(doc, root, "difficulties", draft->difficulties);
    ModEd_PutStr(doc, root, "weekBackground", draft->background);
    ModEd_PutStr(doc, root, "weekBefore", draft->before);
    ModEd_PutBool(doc, root, "startUnlocked", draft->startUnlocked);
    ModEd_PutBool(doc, root, "hideStoryMode", draft->hideStoryMode);
    ModEd_PutBool(doc, root, "hideFreeplay", draft->hideFreeplay);

    yyjson_mut_obj_remove_key(root, "weekCharacters");
    yyjson_mut_val *chars = yyjson_mut_obj_add_arr(doc, root, "weekCharacters");
    for (int i = 0; i < 3; ++i)
        yyjson_mut_arr_add_strcpy(doc, chars, draft->chars[i]);

    yyjson_mut_obj_remove_key(root, "songs");
    yyjson_mut_val *songs = yyjson_mut_obj_add_arr(doc, root, "songs");
    for (int i = 0; i < draft->songCount; ++i) {
        const WeekEdSong *song = &draft->songs[i];
        if (!song->name[0])
            continue;

        yyjson_mut_val *item = yyjson_mut_arr_add_arr(doc, songs);
        yyjson_mut_arr_add_strcpy(doc, item, song->name);
        yyjson_mut_arr_add_strcpy(doc, item, song->icon);
        yyjson_mut_val *color = yyjson_mut_arr_add_arr(doc, item);
        for (int c = 0; c < 3; ++c)
            yyjson_mut_arr_add_int(doc, color, Clamp255(song->color[c]));
    }

    const bool ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);

    if (msg && msgCap) {
        if (ok)
            snprintf(msg, msgCap, "salvo: %s", path);
        else
            snprintf(msg, msgCap, "ERRO ao salvar: %s", path);
    }
    return ok;
}
