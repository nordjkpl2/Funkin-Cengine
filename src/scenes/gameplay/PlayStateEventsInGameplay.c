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
#include "core/ModPath.h"
#include "Render.h"
#include "Log.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "rendering/shaders/ShaderFx.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f
#define SHOW_IMAGE_NAME "Show Image"
#define HIDE_IMAGE_NAME "Hide Image"
#define IMG_MAX 8  /* imagens distintas por musica */

const PlayStateEventType playStateEventTypes[] = {
    {"Fade In",       "tela preta -> imagem (duracao ms, alpha final %)", 1000, 0,   0,   "tempo", "fade",     NULL,       0},
    {"Fade Out",      "imagem -> tela preta (duracao ms, alpha final %)", 1000, 100, 0,   "tempo", "fade",     NULL,       0},
    {SHOW_IMAGE_NAME, "mostra imagem (x, y a partir do centro em px; escala %)", 0, 0, 100, "x (px):", "y (px):", "escala %:", 1},
    {HIDE_IMAGE_NAME, "esconde a imagem mostrada (sem valores)", 0, 0, 0, NULL, NULL, NULL, 0},
    {"Shader TV",     "TV velha: linhas, vinheta, granulado", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Glitch", "glitch: fatias, separacao RGB, blocos", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Off",    "desliga o shader de tela", 0, 0, 0, NULL, NULL, NULL, 0, 1},
};
const int playStateEventTypeCount = (int)(sizeof(playStateEventTypes) / sizeof(playStateEventTypes[0]));

typedef struct {
    double timeMs;
    char name[48];
    double v1;      /* fade: duracao (ms) | imagem: x */
    double v2;      /* fade: alpha final (%) | imagem: y */
    double v3;      /* imagem: escala (%) */
    char img[64];   /* imagem: nome em images/<img>.png */
} PsEvent;

typedef struct {
    char name[64];
    Texture2D tex;
    char ok;        /* textura carregada */
} PsImage;

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

/* Ultimo Show Image disparado (-1 = nenhum). Derivado da ordem dos eventos, entao seek/restart batem. */
static PsImage imgs[IMG_MAX];
static int imgCount;
static int curImg = -1;
static double curX, curY, curScale;

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

static int FindImg(const char* name) {
    for (int i = 0; i < imgCount; i++)
        if (strcmp(imgs[i].name, name) == 0) return i;
    return -1;
}

void PlayStateEvents_ApplyShader(const char* name, double intensityPct, double animPct) {
    const float amt = (float)(intensityPct / 100.0);
    const float spd = (float)(animPct / 100.0);
    if (strcmp(name, "Shader TV") == 0) ShaderFx_Set(SHADERFX_TV, amt, spd);
    else if (strcmp(name, "Shader Glitch") == 0) ShaderFx_Set(SHADERFX_GLITCH, amt, spd);
    else if (strcmp(name, "Shader Off") == 0) ShaderFx_Reset();
}

static void Fire(const PsEvent* e) {
    if (strncmp(e->name, "Shader ", 7) == 0) {
        PlayStateEvents_ApplyShader(e->name, e->v1, e->v2);
        return;
    }
    if (strcmp(e->name, SHOW_IMAGE_NAME) == 0) {
        curImg = FindImg(e->img); /* -1 se o nome estiver vazio ou a imagem nao existir */
        curX = e->v1;
        curY = e->v2;
        curScale = e->v3 > 0.0 ? e->v3 : 100.0;
        return;
    }
    if (strcmp(e->name, HIDE_IMAGE_NAME) == 0) {
        curImg = -1;
        return;
    }

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

/* <mod>/images/<nome>.png, depois assets/images/<nome>.png (mesma regra do stage). */
static int ResolveImage(const char* root, const char* name, char out[512]) {
    if (root[0] != '\0') {
        snprintf(out, 512, "%s/images/%s.png", root, name);
        if (FileExists(out)) return 1;
    }
    snprintf(out, 512, "assets/images/%s.png", name);
    return FileExists(out);
}

/* Carrega (uma vez por nome) as imagens usadas pelos Show Image. Roda no Load, nunca no Draw. */
static void LoadImages(const char* songDir) {
    char root[256] = "";
    (void)ModPath_Root(songDir, root, sizeof(root));

    /* Pede a decodificacao em fundo de todas antes de subir para a GPU. */
    for (int i = 0; i < evCount; i++) {
        if (strcmp(evs[i].name, SHOW_IMAGE_NAME) != 0 || evs[i].img[0] == '\0') continue;
        if (FindImg(evs[i].img) >= 0 || imgCount >= IMG_MAX) continue;

        PsImage* slot = &imgs[imgCount++];
        snprintf(slot->name, sizeof(slot->name), "%s", evs[i].img);
        slot->ok = 0;

        char png[512];
        if (!ResolveImage(root, evs[i].img, png)) {
            Log_WarnF("Show Image: imagem nao encontrada: %s", evs[i].img);
            continue;
        }
        ImagePrefetch_Start(png);
        slot->tex = Render_LoadTexture(png);
        slot->ok = (slot->tex.id != 0);
    }
}

void PlayStateEvents_Unload(void) {
    for (int i = 0; i < imgCount; i++)
        if (imgs[i].ok) UnloadTexture(imgs[i].tex);
    imgCount = 0;
    curImg = -1;

    free(evs);
    evs = NULL;
    evCount = 0;
    nextIdx = 0;
    curAlpha = 0.0f;
    hasFade = 0;
    ShaderFx_Reset();
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
                    /* Show Image: escala ausente = 100 %; nome da imagem na posicao 4 */
                    yyjson_val* j3 = yyjson_arr_get(ev, 3);
                    e->v3 = (j3 != NULL && (yyjson_is_num(j3) || yyjson_is_str(j3))) ? NumOf(j3) : 100.0;
                    yyjson_val* j4 = yyjson_arr_get(ev, 4);
                    e->img[0] = '\0';
                    if (j4 != NULL && yyjson_is_str(j4))
                        snprintf(e->img, sizeof(e->img), "%s", yyjson_get_str(j4));
                }
            }
            qsort(evs, (size_t)evCount, sizeof(PsEvent), CmpEvent);
            LoadImages(songDir);
        }
    }
    yyjson_doc_free(doc);
}

void PlayStateEvents_Reset(void) {
    nextIdx = 0;
    curAlpha = 0.0f;
    hasFade = 0;
    curImg = -1;
    ShaderFx_Reset();
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
    /* Imagem primeiro: o overlay preto do fade fica por cima (Fade In revela a imagem). */
    if (curImg >= 0 && imgs[curImg].ok) {
        const Texture2D t = imgs[curImg].tex;
        const float s = (float)(curScale / 100.0);
        const float w = (float)t.width * s;
        const float h = (float)t.height * s;
        const Rectangle dst = {
            SCREEN_W * 0.5f + (float)curX - w * 0.5f,
            SCREEN_H * 0.5f + (float)curY - h * 0.5f,
            w, h
        };
        DrawTexturePro(t, (Rectangle){0.0f, 0.0f, (float)t.width, (float)t.height}, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }

    if (curAlpha <= 0.001f) return;
    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H,
                  (Color){0, 0, 0, (unsigned char)(curAlpha * 255.0f)});
}
