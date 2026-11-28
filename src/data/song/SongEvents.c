/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SongEvents.c - leitura dos eventos no formato Psych (events.json e chart).
 */

#include "data/song/SongEvents.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SongEvents_ValStr(yyjson_val* v, char* out, size_t n) {
    out[0] = 0;
    if (yyjson_is_str(v)) snprintf(out, n, "%s", yyjson_get_str(v));
    else if (yyjson_is_num(v)) snprintf(out, n, "%g", yyjson_get_num(v));
    else if (yyjson_is_bool(v)) snprintf(out, n, "%s", yyjson_get_bool(v) ? "true" : "false");
}

/* [[tempo, [[nome, v1, v2, v3, s1], ...]], ...] ou [[tempo, nome, v1, v2], ...] */
static int ReadList(yyjson_val* arr, SongEventsFn fn, void* user) {
    if (!yyjson_is_arr(arr)) return 0;
    int n = 0;
    size_t i, imax;
    yyjson_val* item;
    yyjson_arr_foreach(arr, i, imax, item) {
        yyjson_val* jt = yyjson_arr_get(item, 0);
        yyjson_val* second = yyjson_arr_get(item, 1);
        if (!yyjson_is_num(jt)) continue;
        const double t = yyjson_get_num(jt);
        if (yyjson_is_str(second)) {   /* forma antiga: um evento por item */
            yyjson_val* const vals[4] = {yyjson_arr_get(item, 2), yyjson_arr_get(item, 3), NULL, NULL};
            fn(user, t, yyjson_get_str(second), vals);
            n++;
            continue;
        }
        if (!yyjson_is_arr(second)) continue;
        size_t k, kmax;
        yyjson_val* ev;
        yyjson_arr_foreach(second, k, kmax, ev) {
            yyjson_val* jname = yyjson_arr_get(ev, 0);
            if (!yyjson_is_str(jname)) continue;
            yyjson_val* const vals[4] = {yyjson_arr_get(ev, 1), yyjson_arr_get(ev, 2),
                                         yyjson_arr_get(ev, 3), yyjson_arr_get(ev, 4)};
            fn(user, t, yyjson_get_str(jname), vals);
            n++;
        }
    }
    return n;
}

/* Notas legadas: [tempo, -1, nome, v1, v2] dentro de notes[].sectionNotes. */
static int ReadLegacy(yyjson_val* sections, SongEventsFn fn, void* user) {
    if (!yyjson_is_arr(sections)) return 0;
    int n = 0;
    size_t i, imax;
    yyjson_val* sec;
    yyjson_arr_foreach(sections, i, imax, sec) {
        size_t k, kmax;
        yyjson_val* note;
        yyjson_arr_foreach(yyjson_obj_get(sec, "sectionNotes"), k, kmax, note) {
            yyjson_val* jt = yyjson_arr_get(note, 0);
            yyjson_val* lane = yyjson_arr_get(note, 1);
            yyjson_val* jname = yyjson_arr_get(note, 2);
            if (!yyjson_is_num(jt) || !yyjson_is_num(lane) || yyjson_get_num(lane) >= 0.0 || !yyjson_is_str(jname))
                continue;
            yyjson_val* const vals[4] = {yyjson_arr_get(note, 3), yyjson_arr_get(note, 4), NULL, NULL};
            fn(user, yyjson_get_num(jt), yyjson_get_str(jname), vals);
            n++;
        }
    }
    return n;
}

int SongEvents_Read(yyjson_val* root, SongEventsFn fn, void* user) {
    if (!yyjson_is_obj(root) || fn == NULL) return 0;
    int n = ReadList(yyjson_obj_get(root, "events"), fn, user);
    n += ReadLegacy(yyjson_obj_get(root, "notes"), fn, user);
    yyjson_val* song = yyjson_obj_get(root, "song");
    if (yyjson_is_obj(song)) {
        n += ReadList(yyjson_obj_get(song, "events"), fn, user);
        n += ReadLegacy(yyjson_obj_get(song, "notes"), fn, user);
    }
    return n;
}

/* "events" ou "-<digitos> , \"" (casa negativa seguida do nome do evento). */
static char LooksLikeEvents(const char* s, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        if (s[i] == '"' && i + 7 < len && memcmp(s + i + 1, "events\"", 7) == 0) return 1;
        if (s[i] != '-' || s[i + 1] < '0' || s[i + 1] > '9') continue;
        size_t j = i + 1;
        while (j < len && s[j] >= '0' && s[j] <= '9') j++;
        while (j < len && (s[j] == ' ' || s[j] == '\t' || s[j] == '\r' || s[j] == '\n')) j++;
        if (j >= len || s[j] != ',') continue;
        j++;
        while (j < len && (s[j] == ' ' || s[j] == '\t' || s[j] == '\r' || s[j] == '\n')) j++;
        if (j < len && s[j] == '"') return 1;
    }
    return 0;
}

int SongEvents_ReadFile(const char* path, char quickCheck, SongEventsFn fn, void* user) {
    if (path == NULL || path[0] == 0) return 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) return 0;
    char* buf = NULL;
    long size = 0;
    if (fseek(f, 0, SEEK_END) == 0) size = ftell(f);
    if (size > 0 && fseek(f, 0, SEEK_SET) == 0) {
        buf = malloc((size_t)size);
        if (buf != NULL && fread(buf, 1, (size_t)size, f) != (size_t)size) {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    if (buf == NULL) return 0;

    int n = 0;
    if (!quickCheck || LooksLikeEvents(buf, (size_t)size)) {
        yyjson_doc* doc = yyjson_read(buf, (size_t)size, 0);
        if (doc != NULL) {
            n = SongEvents_Read(yyjson_doc_get_root(doc), fn, user);
            yyjson_doc_free(doc);
        }
    }
    free(buf);
    return n;
}

static char FileThere(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

int SongEvents_ReadChartExtras(const char* songDir, const char* chartPath, SongEventsFn fn, void* user) {
    if (chartPath == NULL || chartPath[0] == 0) return 0;
    int n = SongEvents_ReadFile(chartPath, 1, fn, user);

    char own[1024];
    snprintf(own, sizeof(own), "%s/events.json", songDir != NULL ? songDir : "");
    if (songDir != NULL && songDir[0] != 0 && FileThere(own)) return n;   /* o da pasta da musica vale */

    char other[1024];
    snprintf(other, sizeof(other), "%s", chartPath);
    char* cut = strrchr(other, '/');
    char* bs = strrchr(other, '\\');
    if (bs != NULL && (cut == NULL || bs > cut)) cut = bs;
    if (cut == NULL) return n;
    snprintf(cut, sizeof(other) - (size_t)(cut - other), "/events.json");
    if (strcmp(other, own) != 0)
        n += SongEvents_ReadFile(other, 0, fn, user);
    return n;
}
