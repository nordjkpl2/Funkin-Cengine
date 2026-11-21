/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.c - eventos do events.json tocados no PlayState.
 */

#include "scenes/gameplay/PlayStateEventsInGameplay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "raylib.h"
#include "yyjson.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

const PlayStateEventType playStateEventTypes[] = {
    {"Fade In",  "tela preta -> imagem (duracao ms, alpha final %)", 1000, 0},
    {"Fade Out", "imagem -> tela preta (duracao ms, alpha final %)", 1000, 100},
};
const int playStateEventTypeCount = (int)(sizeof(playStateEventTypes) / sizeof(playStateEventTypes[0]));

typedef struct {
    double timeMs;
    char name[48];
    double v1; /* duracao (ms) */
    double v2; /* alpha final (%) */
} PsEvent;

static PsEvent* evs;
static int evCount;
static int nextIdx;    /* proximo evento a disparar */
static float curAlpha; /* alpha do overlay preto (0..1) neste instante */

/* Ultimo fade disparado: o alpha e uma funcao do tempo desse fade. */
static char hasFade;
static double fadeStartMs;
static double fadeDurMs;
static float fadeFrom;
static float fadeTo;

static double NumOf(yyjson_val* v) {
    if (yyjson_is_real(v)) return yyjson_get_real(v);
    if (yyjson_is_sint(v)) return (double)yyjson_get_sint(v);
    if (yyjson_is_uint(v)) return (double)yyjson_get_uint(v);
    if (yyjson_is_str(v)) return atof(yyjson_get_str(v));
    return 0.0;
}

static int CmpEvent(const void* a, const void* b) {
    const double x = ((const PsEvent*)a)->timeMs;
    const double y = ((const PsEvent*)b)->timeMs;
    return (x > y) - (x < y);
}

static float PctToAlpha(double pct) {
    if (pct <= 0.0) return 0.0f;
    if (pct >= 100.0) return 1.0f;
    return (float)(pct / 100.0);
}

/* Alpha do overlay no instante ms, dado o ultimo fade disparado. */
static float AlphaAt(double ms) {
    if (!hasFade) return 0.0f;
    if (fadeDurMs <= 0.0 || ms >= fadeStartMs + fadeDurMs) return fadeTo;
    if (ms <= fadeStartMs) return fadeFrom;
    const double t = (ms - fadeStartMs) / fadeDurMs;
    return fadeFrom + (fadeTo - fadeFrom) * (float)t;
}

static void Fire(const PsEvent* e) {
    float from;
    if (strcmp(e->name, "Fade In") == 0)
        from = 1.0f;                     /* comeca preto e clareia */
    else if (strcmp(e->name, "Fade Out") == 0)
        from = AlphaAt(e->timeMs);       /* escurece a partir de onde esta */
    else
        return;                          /* evento desconhecido: ignorado na gameplay */

    hasFade = 1;
    fadeStartMs = e->timeMs;
    fadeDurMs = e->v1 > 0.0 ? e->v1 : 0.0;
    fadeFrom = from;
    fadeTo = PctToAlpha(e->v2);
}

void PlayStateEvents_Unload(void) {
    free(evs);
    evs = NULL;
    evCount = 0;
    nextIdx = 0;
    curAlpha = 0.0f;
    hasFade = 0;
}

void PlayStateEvents_Load(const char* songDir) {
    PlayStateEvents_Unload();

    char path[512];
    snprintf(path, sizeof(path), "%s/events.json", songDir);
    yyjson_doc* doc = yyjson_read_file(path, 0, NULL, NULL);
    if (doc == NULL) return;

    yyjson_val* arr = yyjson_obj_get(yyjson_doc_get_root(doc), "events");
    if (yyjson_is_arr(arr) && yyjson_arr_size(arr) > 0) {
        evs = malloc(sizeof(PsEvent) * yyjson_arr_size(arr));
        if (evs != NULL) {
            size_t i, imax;
            yyjson_val* item;
            yyjson_arr_foreach(arr, i, imax, item) {
                yyjson_val* jt = yyjson_arr_get(item, 0);
                yyjson_val* jlist = yyjson_arr_get(item, 1);
                if (!yyjson_is_num(jt) || !yyjson_is_arr(jlist)) continue;
                size_t k, kmax;
                yyjson_val* ev;
                yyjson_arr_foreach(jlist, k, kmax, ev) {
                    if (!yyjson_is_arr(ev)) continue;
                    yyjson_val* jname = yyjson_arr_get(ev, 0);
                    if (!yyjson_is_str(jname)) continue;
                    PsEvent* e = &evs[evCount++];
                    e->timeMs = NumOf(jt);
                    snprintf(e->name, sizeof(e->name), "%s", yyjson_get_str(jname));
                    e->v1 = NumOf(yyjson_arr_get(ev, 1));
                    e->v2 = NumOf(yyjson_arr_get(ev, 2));
                }
            }
            qsort(evs, (size_t)evCount, sizeof(PsEvent), CmpEvent);
        }
    }
    yyjson_doc_free(doc);
}

void PlayStateEvents_Reset(void) {
    nextIdx = 0;
    curAlpha = 0.0f;
    hasFade = 0;
}

void PlayStateEvents_SeekTo(double seconds) {
    const double ms = seconds * 1000.0;
    PlayStateEvents_Reset();
    while (nextIdx < evCount && evs[nextIdx].timeMs <= ms) {
        Fire(&evs[nextIdx]);
        nextIdx++;
    }
    curAlpha = AlphaAt(ms);
}

void PlayStateEvents_Update(double seconds) {
    const double ms = seconds * 1000.0;
    while (nextIdx < evCount && evs[nextIdx].timeMs <= ms) {
        Fire(&evs[nextIdx]);
        nextIdx++;
    }
    curAlpha = AlphaAt(ms);
}

void PlayStateEvents_Draw(void) {
    if (curAlpha <= 0.001f) return;
    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H,
                  (Color){0, 0, 0, (unsigned char)(curAlpha * 255.0f)});
}
