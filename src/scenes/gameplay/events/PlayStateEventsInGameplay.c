/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.c - eventos do events.json tocados no PlayState.
 */

#include "scenes/gameplay/events/PlayStateEventsInGameplay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "raylib.h"
#include "yyjson.h"
#include "core/ModPath.h"
#include "Render.h"
#include "core/Log.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "rendering/shaders/ShaderFx.h"
#include "scenes/gameplay/characters/PlayStateAnimations.h"
#include "scenes/api/FontLoader.h"

#define SCREEN_W 1280.0f
#include "scenes/gameplay/events/PlayStateParallax.h"
#include "scenes/gameplay/events/PlayStateCameraMove.h"
#define SCREEN_H 720.0f
#define SHOW_IMAGE_NAME "Show Image"
#define HIDE_IMAGE_NAME "Hide Image"
#define CHANGE_CHAR_NAME "Change Character"
#define CAMERA_PARALLAX_NAME "Camera Parallax"
#define CAMERA_MOVE_NAME "Camera Move"
#define LETTERBOX_NAME "Letterbox"
#define MUSIC_CREDIT_NAME "Music Credit"
#define CREDIT_DEFAULT_MS 4000.0
#define CREDIT_SLIDE_FRAC 0.12  /* fracao da duracao gasta entrando e saindo */
#define LETTERBOX_MAX_FRAC 0.5f  /* cada barra no maximo metade da tela */
#define IMG_MAX 8  /* imagens distintas por musica */

const PlayStateEventType playStateEventTypes[] = {
    {"Fade In",       "tela preta -> imagem (duracao ms, alpha final %)", 1000, 0,   0,   "tempo", "fade",     NULL,       0},
    {"Fade Out",      "imagem -> tela preta (duracao ms, alpha final %)", 1000, 100, 0,   "tempo", "fade",     NULL,       0},
    {SHOW_IMAGE_NAME, "mostra imagem (x, y a partir do centro em px; escala %)", 0, 0, 100, "x (px):", "y (px):", "escala %:", 1},
    {HIDE_IMAGE_NAME, "esconde a imagem mostrada (sem valores)", 0, 0, 0, NULL, NULL, NULL, 0},
    {"Shader TV",     "TV velha: linhas, vinheta, granulado", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Glitch", "glitch: fatias, separacao RGB, blocos", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Blackness", "buraco negro: puxa pro centro, escurece", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Grayscale", "tudo em preto e branco (intensidade % = quanto da cor some; animacao sem efeito)", 100, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Bloom",  "brilho: pixels claros ganham halo (intensidade % = forca e raio)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Space",  "espaco: nebulosa colorida + estrelas descendo e piscando (intensidade % = forca, animacao % = velocidade)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Pulse",  "pulso: batida tum-tum no ritmo (zoom + clarao + borda; animacao % = BPM: 100 = 120)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Distortion", "distorcao: ondas de calor + curvatura de lente (intensidade % = forca, animacao % = velocidade)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Chromatic", "aberracao cromatica: separa vermelho e azul do centro (intensidade % = forca, animacao % = pulso)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Dissolve", "dissolver: pedacos da tela somem por ruido, borda laranja (intensidade % = quanto sumiu, animacao % = velocidade)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Depth", "profundidade: paralaxe pelo brilho + nevoa no fundo (intensidade % = forca, animacao % = balanco da camera)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Wave",   "ondulacao do fundo do stage (nao conta nos 3 shaders; intensidade % = forca, animacao % = velocidade)", 50, 0, 0, "intensidade %:", "animacao %:", NULL, 0, 1},
    {"Shader Off",    "desliga o shader de tela e a ondulacao do fundo", 0, 0, 0, NULL, NULL, NULL, 0, 1},
    {"Camera Parallax", "camera 3D: afasta (zoom out) e balanca em profundidade (0 desliga)", 100, 0, 0, "intensidade %:", "velocidade %:", NULL, 0, 0},
    {CAMERA_MOVE_NAME, "camera: zoom % (100 = normal), x e y em px do mundo (transicao suave)", 100, 0, 0, "zoom %:", "x (px):", "y (px):", 0, 0},
    {CHANGE_CHAR_NAME, "troca o personagem: 0=oponente 1=bf 2=gf (nome do personagem)", 0, 0, 0, "alvo (0/1/2):", NULL, NULL, 0, 0, 1},
    {LETTERBOX_NAME, "barras de filme preto em cima e embaixo (duracao ms, altura % da tela; 0 tira)", 1000, 12, 0, "tempo", "altura %:", NULL, 0},
    {MUSIC_CREDIT_NAME, "disco com titulo da musica e criador (duracao ms; texto = nome do criador)", 4000, 0, 0, "tempo (ms):", NULL, NULL, 0, 0, 0, 1},
};
const int playStateEventTypeCount = (int)(sizeof(playStateEventTypes) / sizeof(playStateEventTypes[0]));

typedef struct {
    double timeMs;
    char name[48];
    double v1;      /* fade: duracao (ms) | imagem: x */
    double v2;      /* fade: alpha final (%) | imagem: y */
    double v3;      /* imagem: escala (%) */
    char img[64];   /* imagem: nome em images/<img>.png | Change Character: nome do personagem */
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

/* Ultimo Letterbox disparado: altura de cada barra (fracao da tela) anima de from a to. */
static char hasLb;
static double lbStartMs;
static double lbDurMs;
static float lbFrom;
static float lbTo;
static float curBar;   /* altura atual de cada barra (fracao da tela) neste instante */

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

/* Music Credit: ultimo disparado. Visivel so enquanto curMs esta dentro da janela. */
static double curMs;
static char hasCredit;
static double creditStartMs;
static double creditDurMs;
static char creditTitle[64];   /* nome da pasta da musica (Load) */
static char creditName[64];    /* criador (s1 do evento) */

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

/* Altura da barra no instante ms, dado o ultimo Letterbox disparado. */
static float BarAt(double ms) {
    if (!hasLb) return 0.0f;
    if (lbDurMs <= 0.0 || ms >= lbStartMs + lbDurMs) return lbTo;
    if (ms <= lbStartMs) return lbFrom;
    const double t = (ms - lbStartMs) / lbDurMs;
    return lbFrom + (lbTo - lbFrom) * (float)t;
}

static float PctToBar(double pct) {
    if (pct <= 0.0) return 0.0f;
    if (pct >= 50.0) return LETTERBOX_MAX_FRAC;
    return (float)(pct / 100.0);
}

static int FindImg(const char* name) {
    for (int i = 0; i < imgCount; i++)
        if (strcmp(imgs[i].name, name) == 0) return i;
    return -1;
}

void PlayStateEvents_ApplyShader(const char* name, double intensityPct, double animPct) {
    const float amt = (float)(intensityPct / 100.0);
    const float spd = (float)(animPct / 100.0);
    /* Empilha: efeitos novos somam aos que ja estao ligados (max 3). "Shader Off" limpa tudo. */
    if (strcmp(name, "Shader TV") == 0) ShaderFx_Push(SHADERFX_TV, amt, spd);
    else if (strcmp(name, "Shader Glitch") == 0) ShaderFx_Push(SHADERFX_GLITCH, amt, spd);
    else if (strcmp(name, "Shader Blackness") == 0) ShaderFx_Push(SHADERFX_BLACKNESS, amt, spd);
    else if (strcmp(name, "Shader Grayscale") == 0) ShaderFx_Push(SHADERFX_GRAYSCALE, amt, spd);
    else if (strcmp(name, "Shader Bloom") == 0) ShaderFx_Push(SHADERFX_BLOOM, amt, spd);
    else if (strcmp(name, "Shader Space") == 0) ShaderFx_Push(SHADERFX_SPACE, amt, spd);
    else if (strcmp(name, "Shader Pulse") == 0) ShaderFx_Push(SHADERFX_PULSE, amt, spd);
    else if (strcmp(name, "Shader Distortion") == 0) ShaderFx_Push(SHADERFX_DISTORTION, amt, spd);
    else if (strcmp(name, "Shader Chromatic") == 0) ShaderFx_Push(SHADERFX_CHROMATIC, amt, spd);
    else if (strcmp(name, "Shader Dissolve") == 0) ShaderFx_Push(SHADERFX_DISSOLVE, amt, spd);
    else if (strcmp(name, "Shader Depth") == 0) ShaderFx_Push(SHADERFX_DEPTH, amt, spd);
    else if (strcmp(name, "Shader Wave") == 0) ShaderFx_SetBackgroundWave(amt, spd);   /* fora da pilha */
    else if (strcmp(name, "Shader Off") == 0) ShaderFx_Reset();                        /* limpa pilha e ondulacao */
}

static void Fire(const PsEvent* e) {
    if (strncmp(e->name, "Shader ", 7) == 0) {
        PlayStateEvents_ApplyShader(e->name, e->v1, e->v2);
        return;
    }
    if (strcmp(e->name, CAMERA_PARALLAX_NAME) == 0) {
        PlayStateParallax_Set((float)(e->v1 / 100.0), (float)(e->v2 > 0.0 ? e->v2 / 100.0 : 1.0));
        return;
    }
    if (strcmp(e->name, CAMERA_MOVE_NAME) == 0) {
        PlayStateCameraMove_Set((float)e->v1, (float)e->v2, (float)e->v3);
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
    if (strcmp(e->name, CHANGE_CHAR_NAME) == 0) {
        PlayStateAnimations_ChangeCharacter((int)e->v1, e->img);
        return;
    }
    if (strcmp(e->name, MUSIC_CREDIT_NAME) == 0) {
        hasCredit = 1;
        creditStartMs = e->timeMs;
        creditDurMs = e->v1 > 0.0 ? e->v1 : CREDIT_DEFAULT_MS;
        snprintf(creditName, sizeof(creditName), "%s", e->img);
        return;
    }
    if (strcmp(e->name, LETTERBOX_NAME) == 0) {
        hasLb = 1;
        lbStartMs = e->timeMs;
        lbDurMs = e->v1 > 0.0 ? e->v1 : 0.0;
        lbFrom = BarAt(e->timeMs);       /* anima a partir de onde esta */
        lbTo = PctToBar(e->v2);
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
    hasLb = 0;
    curBar = 0.0f;
    hasCredit = 0;
    ShaderFx_Reset();
    PlayStateParallax_Reset();
    PlayStateCameraMove_Reset();
}

void PlayStateEvents_Load(const char* songDir) {
    PlayStateEvents_Unload();

    /* Titulo do Music Credit: ultimo segmento do caminho da pasta da musica. */
    const char* base = strrchr(songDir, '/');
    const char* bslash = strrchr(songDir, '\\');
    if (bslash != NULL && (base == NULL || bslash > base)) base = bslash;
    snprintf(creditTitle, sizeof(creditTitle), "%s", base != NULL ? base + 1 : songDir);

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
                    if (strcmp(e->name, CAMERA_MOVE_NAME) == 0 && !(j3 != NULL && yyjson_is_num(j3)))
                        e->v3 = 0.0; /* Camera Move: y ausente = 0 px (nao 100) */
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
    hasLb = 0;
    curBar = 0.0f;
    curImg = -1;
    hasCredit = 0;
    curMs = 0.0;
    ShaderFx_Reset();
    PlayStateParallax_Reset();
    PlayStateCameraMove_Reset();
}

void PlayStateEvents_SeekTo(double seconds) {
    const double ms = seconds * 1000.0;
    PlayStateEvents_Reset();
    curMs = ms;
    while (nextIdx < evCount && evs[nextIdx].timeMs <= ms) {
        Fire(&evs[nextIdx]);
        nextIdx++;
    }
    curAlpha = AlphaAt(ms);
    curBar = BarAt(ms);
}

void PlayStateEvents_Update(double seconds) {
    const double ms = seconds * 1000.0;
    curMs = ms;
    while (nextIdx < evCount && evs[nextIdx].timeMs <= ms) {
        Fire(&evs[nextIdx]);
        nextIdx++;
    }
    curAlpha = AlphaAt(ms);
    curBar = BarAt(ms);
}

/* Disco de vinil desenhado por codigo (sem asset). Centro (cx, cy), raio r, angulo em graus. */
static void DrawRecord(float cx, float cy, float r, float angle, float alpha) {
    const unsigned char a = (unsigned char)(alpha * 255.0f);
    DrawCircleV((Vector2){cx, cy}, r, (Color){18, 18, 18, a});
    for (float g = r * 0.34f; g < r * 0.97f; g += 7.0f)          /* sulcos */
        DrawRing((Vector2){cx, cy}, g, g + 1.5f, 0.0f, 360.0f, 72, (Color){55, 55, 55, a});
    DrawCircleSector((Vector2){cx, cy}, r * 0.95f, angle, angle + 35.0f, 24,
                     (Color){255, 255, 255, (unsigned char)(28.0f * alpha)});  /* brilho girando */
    DrawCircleV((Vector2){cx, cy}, r * 0.36f, (Color){200, 50, 60, a});     /* etiqueta */
    DrawCircleV((Vector2){cx, cy}, r * 0.05f, (Color){18, 18, 18, a});      /* furo central */
}

/* Music Credit: disco entra pela direita, gira, mostra titulo e criador, e sai. */
static void DrawMusicCredit(void) {
    if (!hasCredit || creditDurMs <= 0.0) return;
    const double t = curMs - creditStartMs;
    if (t < 0.0 || t >= creditDurMs) return;

    const double slideMs = creditDurMs * CREDIT_SLIDE_FRAC;
    float p = 1.0f;                                  /* 0 = fora da tela, 1 = em posicao */
    if (t < slideMs) p = (float)(t / slideMs);
    else if (t > creditDurMs - slideMs) p = (float)((creditDurMs - t) / slideMs);
    p = p * p * (3.0f - 2.0f * p);                   /* suaviza entrada e saida */

    const float R = 210.0f;
    const float cx = SCREEN_W * 0.30f + (1.0f - p) * 600.0f;   /* desliza da direita */
    const float cy = SCREEN_H * 0.5f;
    const float angle = (float)(t * 0.12);           /* 120 graus por segundo */

    DrawRecord(cx, cy, R, angle, p);

    const float tx = cx + R + 40.0f;
    const float ty = cy - 50.0f;
    const Color titleCol = Fade(WHITE, p);
    const Color subCol = Fade((Color){220, 220, 220, 255}, p);
    DrawTextEx(mainFont, creditTitle, (Vector2){tx, ty}, 56.0f, 2.0f, titleCol);
    DrawTextEx(mainFont, "by",(Vector2){tx, ty + 70.0f}, 26.0f, 1.0f, subCol);
    DrawTextEx(mainFont, creditName, (Vector2){tx, ty + 100.0f}, 36.0f, 1.0f, titleCol);
}

void PlayStateEvents_Draw(void) {
    DrawMusicCredit();

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

    if (curAlpha > 0.001f)
        DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H,
                      (Color){0, 0, 0, (unsigned char)(curAlpha * 255.0f)});
}

void PlayStateEvents_DrawLetterbox(void) {
    /* Barras pretas por cima do mundo, mas por baixo do HUD/notas para nao cobri-las. */
    if (curBar > 0.001f) {
        const int bh = (int)(curBar * SCREEN_H + 0.5f);
        DrawRectangle(0, 0, (int)SCREEN_W, bh, BLACK);
        DrawRectangle(0, (int)SCREEN_H - bh, (int)SCREEN_W, bh, BLACK);
    }
}
