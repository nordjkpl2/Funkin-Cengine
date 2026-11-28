/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: eventos da musica (events.json). Lista ordenada por tempo. */

#include "scenes/modding/charteditor/model/EditorEvents.h"
#include "yyjson.h"
#include "data/song/SongEvents.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EdEvent* edEvents = NULL;
int edEventCount = 0;
int edEventsGen = 0;
static int edEventCap = 0;

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

/* Change Character da Psych ([nome, bf/dad/gf ou 0/1/2, personagem]) -> formato do engine
 * (v1 = 0 oponente / 1 BF / 2 GF, s1 = personagem). O campo numerico do editor estragaria "dad". */
static void EdEvent_PsychChangeChar(char* a, size_t an, char* b, size_t bn, char* s, size_t sn) {
    char t[16];
    size_t k = 0;
    for(const char* p = a; *p != 0 && k + 1 < sizeof(t); p++)
        if(*p != ' ') t[k++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
    t[k] = 0;
    int target = 1;   /* Psych: padrao = BF */
    if(strcmp(t, "gf") == 0 || strcmp(t, "girlfriend") == 0 || strcmp(t, "2") == 0) target = 2;
    else if(strcmp(t, "dad") == 0 || strcmp(t, "opponent") == 0 || strcmp(t, "1") == 0) target = 0;
    snprintf(s, sn, "%s", b);
    snprintf(a, an, "%d", target);
    snprintf(b, bn, "0");
}

/* user != NULL (eventos do chart): evento igual a um ja carregado e pulado
 * (depois de salvar, o events.json ja traz os do chart e o chart original pode continuar com eles). */
static void EdEvents_OnEvent(void* user, double t, const char* name, yyjson_val* const vals[4]) {
    char a[64], b[64], c[16], s[64];
    SongEvents_ValStr(vals[0], a, sizeof(a));
    SongEvents_ValStr(vals[1], b, sizeof(b));
    SongEvents_ValStr(vals[2], c, sizeof(c));
    SongEvents_ValStr(vals[3], s, sizeof(s));
    if(c[0] == 0 && strcmp(name, "Show Image") == 0) snprintf(c, sizeof(c), "100"); /* escala padrao */
    if(vals[3] == NULL && strcmp(name, "Change Character") == 0 && b[0] != 0)
        EdEvent_PsychChangeChar(a, sizeof(a), b, sizeof(b), s, sizeof(s));
    if(name[0] == 0) return;
    for(int i = 0; user != NULL && i < edEventCount; i++) {
        const EdEvent* o = &edEvents[i];
        if(fabsf(o->time - (float)t) < 0.5f && strcmp(o->name, name) == 0 && strcmp(o->v1, a) == 0 &&
           strcmp(o->v2, b) == 0 && strcmp(o->s1, s) == 0)
            return;
    }
    EdEvent_Add((float)t, name, a, b, c, s);
}

void EdEvents_Load(const char* dir) {
    EdEvent_ClearAll();
    char p[300];
    snprintf(p, sizeof(p), "%s/events.json", dir);
    SongEvents_ReadFile(p, 0, EdEvents_OnEvent, NULL);
}

/* Eventos de dentro do chart (Psych: "events" e notas -1). Salvar grava todos no events.json
 * e o chart sai sem eles, entao nao duplicam. */
void EdEvents_AddFromChart(const char* dir, const char* chartPath) {
    int fromChart = 1;   /* so precisa ser != NULL */
    if(SongEvents_ReadChartExtras(dir, chartPath, EdEvents_OnEvent, &fromChart) > 0)
        edEventsGen++;
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

    bool ok = yyjson_mut_write_file(p, doc, 0, NULL, NULL);  /* compacto */
    yyjson_mut_doc_free(doc);
    return ok ? 1 : 0;
}
