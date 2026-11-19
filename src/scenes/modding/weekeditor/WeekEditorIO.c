/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Week Editor: IO com yyjson. Formato Psych: songs = [["nome", "icone", [r,g,b]], ...]. */
#include "scenes/modding/weekeditor/WeekEditorIO.h"

#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "yyjson.h"

#define WEEKED_SONG_DEFAULT_COLOR "146,113,253"

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
    draft->startUnlocked = true;
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

static void CopyColor(char *dst, size_t cap, yyjson_val *v)
{
    if (yyjson_is_arr(v) && yyjson_arr_size(v) >= 3) {
        snprintf(dst, cap, "%d,%d,%d",
                 (int)Num(yyjson_arr_get(v, 0)), (int)Num(yyjson_arr_get(v, 1)),
                 (int)Num(yyjson_arr_get(v, 2)));
    } else {
        snprintf(dst, cap, "%s", WEEKED_SONG_DEFAULT_COLOR);
    }
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
            CopyStr(song->name, sizeof(song->name), yyjson_arr_get(item, 0));
            CopyStr(song->icon, sizeof(song->icon), yyjson_arr_get(item, 1));
            CopyColor(song->color, sizeof(song->color), yyjson_arr_get(item, 2));
        }
    }

    yyjson_doc_free(doc);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Escrita                                                                   */
/* ------------------------------------------------------------------------- */

/* "r,g,b" -> 0..255 cada um; texto invalido vira o padrao. */
static void ParseColor(const char *text, int rgb[3])
{
    int r = 146, g = 113, b = 253;
    if (sscanf(text, "%d , %d , %d", &r, &g, &b) != 3 && sscanf(text, "%d,%d,%d", &r, &g, &b) != 3) {
        r = 146;
        g = 113;
        b = 253;
    }
    rgb[0] = r < 0 ? 0 : (r > 255 ? 255 : r);
    rgb[1] = g < 0 ? 0 : (g > 255 ? 255 : g);
    rgb[2] = b < 0 ? 0 : (b > 255 ? 255 : b);
}

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

        int rgb[3];
        ParseColor(song->color, rgb);

        yyjson_mut_val *item = yyjson_mut_arr_add_arr(doc, songs);
        yyjson_mut_arr_add_strcpy(doc, item, song->name);
        yyjson_mut_arr_add_strcpy(doc, item, song->icon);
        yyjson_mut_val *color = yyjson_mut_arr_add_arr(doc, item);
        yyjson_mut_arr_add_int(doc, color, rgb[0]);
        yyjson_mut_arr_add_int(doc, color, rgb[1]);
        yyjson_mut_arr_add_int(doc, color, rgb[2]);
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
