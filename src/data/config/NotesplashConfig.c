/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NotesplashConfig.c - carrega, valida e grava assets/notesplashes.json.
 * Entrada invalida cai fora; lista vazia volta ao padrao. O arquivo nunca quebra o jogo.
 */
#include "data/config/NotesplashConfig.h"
#include "yyjson.h"

#include <stdio.h>
#include <string.h>

#define NOTESPLASH_PATH "assets/notesplashes.json"

static NotesplashList current;
static char loaded;

void NotesplashConfig_Defaults(NotesplashList* out) {
    memset(out, 0, sizeof(*out));
    out->count = 1;
    out->active = 0;
    strcpy(out->entries[0].animset, "assets/images/noteSplashes-sparkles.xml");
    strcpy(out->entries[0].prefix, "splash sparkles red ");
}

static void CopyStr(char* dst, size_t size, const char* src) {
    snprintf(dst, size, "%s", src);
}

static void ReadFromJson(yyjson_val* root, NotesplashList* list) {
    yyjson_val* arr = yyjson_obj_get(root, "splashes");
    if (!yyjson_is_arr(arr)) return;

    NotesplashList tmp;
    memset(&tmp, 0, sizeof(tmp));
    yyjson_val* item;
    yyjson_arr_iter it = yyjson_arr_iter_with(arr);
    while ((item = yyjson_arr_iter_next(&it)) != NULL && tmp.count < NOTESPLASH_MAX) {
        const char* animset = yyjson_get_str(yyjson_obj_get(item, "animset"));
        const char* prefix = yyjson_get_str(yyjson_obj_get(item, "prefix"));
        if (animset == NULL || prefix == NULL || prefix[0] == '\0') continue;
        CopyStr(tmp.entries[tmp.count].animset, sizeof(tmp.entries[0].animset), animset);
        CopyStr(tmp.entries[tmp.count].prefix, sizeof(tmp.entries[0].prefix), prefix);
        tmp.count++;
    }
    if (tmp.count == 0) return;

    yyjson_val* act = yyjson_obj_get(root, "active");
    int a = yyjson_is_num(act) ? (int)yyjson_get_num(act) : 0;
    if (a < 0 || a >= tmp.count) a = 0;
    tmp.active = a;
    *list = tmp;
}

static void EnsureLoaded(void) {
    if (loaded) return;
    loaded = 1;
    NotesplashConfig_Defaults(&current);

    yyjson_doc* doc = yyjson_read_file(NOTESPLASH_PATH, 0, NULL, NULL);
    if (doc == NULL) return;
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) ReadFromJson(root, &current);
    yyjson_doc_free(doc);
}

const NotesplashList* NotesplashConfig_Get(void) {
    EnsureLoaded();
    return &current;
}

void NotesplashConfig_Set(const NotesplashList* list) {
    EnsureLoaded();
    if (list != NULL) current = *list;
}

const NotesplashEntry* NotesplashConfig_Active(void) {
    EnsureLoaded();
    return &current.entries[current.active];
}

char NotesplashConfig_Save(void) {
    EnsureLoaded();
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_int(doc, root, "active", current.active);
    yyjson_mut_val* arr = yyjson_mut_obj_add_arr(doc, root, "splashes");
    for (int i = 0; i < current.count; i++) {
        yyjson_mut_val* o = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_strcpy(doc, o, "animset", current.entries[i].animset);
        yyjson_mut_obj_add_strcpy(doc, o, "prefix", current.entries[i].prefix);
        yyjson_mut_arr_add_val(arr, o);
    }

    const bool ok = yyjson_mut_write_file(NOTESPLASH_PATH, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok ? 1 : 0;
}

char NotesplashConfig_TexturePath(const char* animset, char* out, size_t size) {
    if (animset == NULL || out == NULL || size == 0) return 0;
    const size_t len = strlen(animset);
    if (len < 4 || len + 1 > size || strcmp(animset + len - 4, ".xml") != 0) return 0;
    memcpy(out, animset, len - 4);
    memcpy(out + len - 4, ".png", 5);
    return 1;
}
