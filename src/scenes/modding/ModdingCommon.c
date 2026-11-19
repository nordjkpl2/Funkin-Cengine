/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#include "scenes/modding/ModdingCommon.h"

#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "core/ModList.h"

int ModEd_Roots(char out[][MODED_ROOT_MAX], int capacity)
{
    int count = 0;
    if (!out || capacity <= 0)
        return 0;

    ModList_Rescan();
    for (int i = 0; i < ModList_Count() && count < capacity - 1; ++i) {
        if (!ModList_Enabled(i))
            continue;
        snprintf(out[count], MODED_ROOT_MAX, "%s", ModList_Root(i));
        ++count;
    }
    if (count < capacity)
        snprintf(out[count++], MODED_ROOT_MAX, "%s", "assets");
    return count;
}

static bool Stem_Known(char out[][MODED_NAME_MAX], int count, const char *name)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(out[i], name) == 0)
            return true;
    }
    return false;
}

int ModEd_ListStems(const char *dir, const char *ext, char out[][MODED_NAME_MAX], int capacity)
{
    int count = 0;
    if (!dir || !ext || !out || capacity <= 0 || !DirectoryExists(dir))
        return 0;

    FilePathList list = LoadDirectoryFilesEx(dir, ext, false);
    for (unsigned int i = 0; i < list.count && count < capacity; ++i) {
        const char *base = GetFileName(list.paths[i]);
        char stem[MODED_NAME_MAX];
        snprintf(stem, sizeof(stem), "%s", base);

        char *dot = strrchr(stem, '.');
        if (dot)
            *dot = '\0';
        if (!stem[0] || Stem_Known(out, count, stem))
            continue;

        snprintf(out[count++], MODED_NAME_MAX, "%s", stem);
    }
    UnloadDirectoryFiles(list);
    return count;
}

bool ModEd_IsSafeName(const char *name)
{
    if (!name || !name[0] || name[0] == '.' || strlen(name) >= MODED_NAME_MAX)
        return false;

    for (const char *p = name; *p; ++p) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!ok)
            return false;
    }
    return strstr(name, "..") == NULL;
}

bool ModEd_TypeText(char *buf, size_t capacity)
{
    bool changed = false;
    size_t len = strlen(buf);
    int c;

    while ((c = GetCharPressed()) > 0) {
        if (c >= 32 && c <= 126 && len + 1 < capacity) {
            buf[len++] = (char)c;
            buf[len] = '\0';
            changed = true;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && len > 0) {
        buf[--len] = '\0';
        changed = true;
    }
    return changed;
}

bool ModEd_BackupOnce(const char *path)
{
    char backup[MODED_ROOT_MAX + 8];
    if (!FileExists(path))
        return true;

    snprintf(backup, sizeof(backup), "%s.bak", path);
    if (FileExists(backup))
        return true;

    FILE *in = fopen(path, "rb");
    FILE *out = fopen(backup, "wb");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return false;
    }

    char chunk[4096];
    size_t n;
    bool ok = true;
    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    fclose(in);
    fclose(out);
    return ok;
}

void ModEd_PutReal(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, double value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_real(doc, obj, key, value);
}

void ModEd_PutInt(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_int(doc, obj, key, value);
}

void ModEd_PutBool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_bool(doc, obj, key, value);
}

void ModEd_PutStr(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *value)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_obj_add_strcpy(doc, obj, key, value ? value : "");
}

void ModEd_PutRealArr2(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, float x, float y)
{
    yyjson_mut_obj_remove_key(obj, key);
    yyjson_mut_val *arr = yyjson_mut_obj_add_arr(doc, obj, key);
    yyjson_mut_arr_add_real(doc, arr, x);
    yyjson_mut_arr_add_real(doc, arr, y);
}
