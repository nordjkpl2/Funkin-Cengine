/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: eventos da musica (events.json). Lista ordenada por tempo. */

#include "scenes/modding/charteditor/data/EditorEvents.h"
#include "yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EdEvent* edEvents = NULL;
int edEventCount = 0;
int edEventsGen = 0;
static int edEventCap = 0;

static double EvNum(yyjson_val* v) {
    if(yyjson_is_real(v)) return yyjson_get_real(v);
    if(yyjson_is_sint(v)) return (double)yyjson_get_sint(v);
    if(yyjson_is_uint(v)) return (double)yyjson_get_uint(v);
    return 0;
}

void EdEvent_ClearAll(void) {
    free(edEvents);
    edEvents = NULL;
    edEventCount = 0;
    edEventCap = 0;
    edEventsGen++;
}

// insere mantendo ordem por tempo (iguais ficam na ordem de insercao)
int EdEvent_Add(float time, const char* name, const char* v1, const char* v2, const char* v3, const char* s1) {
    if(edEventCount >= edEventCap) {
        int ncap = edEventCap ? edEventCap * 2 : 16;
        EdEvent* n = realloc(edEvents, sizeof(EdEvent) * (size_t)ncap);
        if(n == NULL) return -1;
        edEvents = n;
        edEventCap = ncap;
    }
    int at = edEventCount;
    while(at > 0 && edEvents[at - 1].time > time) at--;
    memmove(edEvents + at + 1, edEvents + at, sizeof(EdEvent) * (size_t)(edEventCount - at));
    EdEvent* e = &edEvents[at];
    memset(e, 0, sizeof(*e));
    e->time = time;
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->v1, sizeof(e->v1), "%s", v1);
    snprintf(e->v2, sizeof(e->v2), "%s", v2);
    snprintf(e->v3, sizeof(e->v3), "%s", v3);
    snprintf(e->s1, sizeof(e->s1), "%s", s1);
    edEventCount++;
    return at;
}

void EdEvent_RemoveAt(int idx) {
    if(idx < 0 || idx >= edEventCount) return;
    memmove(edEvents + idx, edEvents + idx + 1, sizeof(EdEvent) * (size_t)(edEventCount - idx - 1));
    edEventCount--;
}

static void EdEvent_ValStr(yyjson_val* v, char* out, size_t n) {
    out[0] = 0;
    if(yyjson_is_str(v)) snprintf(out, n, "%s", yyjson_get_str(v));
    else if(yyjson_is_num(v)) snprintf(out, n, "%g", EvNum(v));
    else if(yyjson_is_bool(v)) snprintf(out, n, "%s", yyjson_get_bool(v) ? "true" : "false");
}

void EdEvents_Load(const char* dir) {
    EdEvent_ClearAll();
    char p[300];
    snprintf(p, sizeof(p), "%s/events.json", dir);
    yyjson_doc* doc = yyjson_read_file(p, 0, NULL, NULL);
    if(doc == NULL) return;
    yyjson_val* arr = yyjson_obj_get(yyjson_doc_get_root(doc), "events");
    if(yyjson_is_arr(arr)) {
        size_t i, imax;
        yyjson_val* item;
        yyjson_arr_foreach(arr, i, imax, item) {
            yyjson_val* jt = yyjson_arr_get(item, 0);
            yyjson_val* jlist = yyjson_arr_get(item, 1);
            if(!yyjson_is_num(jt) || !yyjson_is_arr(jlist)) continue;
            float t = (float)EvNum(jt);
            size_t k, kmax;
            yyjson_val* ev;
            yyjson_arr_foreach(jlist, k, kmax, ev) {
                if(!yyjson_is_arr(ev)) continue;
                char nm[48], a[64], b[64], c[16], s[64];
                EdEvent_ValStr(yyjson_arr_get(ev, 0), nm, sizeof(nm));
                EdEvent_ValStr(yyjson_arr_get(ev, 1), a, sizeof(a));
                EdEvent_ValStr(yyjson_arr_get(ev, 2), b, sizeof(b));
                EdEvent_ValStr(yyjson_arr_get(ev, 3), c, sizeof(c));
                EdEvent_ValStr(yyjson_arr_get(ev, 4), s, sizeof(s));
                if(c[0] == 0 && strcmp(nm, "Show Image") == 0) snprintf(c, sizeof(c), "100"); /* escala padrao */
                if(nm[0] != 0) EdEvent_Add(t, nm, a, b, c, s);
            }
        }
    }
    yyjson_doc_free(doc);
}

// grava events.json (so cria o arquivo se houver eventos ou se ja existia)
char EdEvents_Save(const char* dir) {
    char p[300];
    snprintf(p, sizeof(p), "%s/events.json", dir);
    if(edEventCount == 0 && !FileExists(p)) return 1;

    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_val* jarr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "events", jarr);

    for(int i = 0; i < edEventCount; i++) {
        const EdEvent* e = edEvents + i;
        yyjson_mut_val* item = yyjson_mut_arr(doc);
        yyjson_mut_arr_append(jarr, item);
        yyjson_mut_arr_append(item, yyjson_mut_real(doc, (double)e->time));
        yyjson_mut_val* list = yyjson_mut_arr(doc);
        yyjson_mut_arr_append(item, list);
        yyjson_mut_val* ev = yyjson_mut_arr(doc);
        yyjson_mut_arr_append(list, ev);
        yyjson_mut_arr_append(ev, yyjson_mut_strcpy(doc, e->name));
        yyjson_mut_arr_append(ev, yyjson_mut_strcpy(doc, e->v1));
        yyjson_mut_arr_append(ev, yyjson_mut_strcpy(doc, e->v2));
        // Show Image: v3 (escala) + s1 (imagem); Change Character: s1 (nome); Music Credit: s1 (criador)
        if(strcmp(e->name, "Show Image") == 0 || strcmp(e->name, "Change Character") == 0 || strcmp(e->name, "Music Credit") == 0 || strcmp(e->name, "Camera Move") == 0) {
            yyjson_mut_arr_append(ev, yyjson_mut_strcpy(doc, e->v3));
            yyjson_mut_arr_append(ev, yyjson_mut_strcpy(doc, e->s1));
        }
    }

    bool ok = yyjson_mut_write_file(p, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok ? 1 : 0;
}
