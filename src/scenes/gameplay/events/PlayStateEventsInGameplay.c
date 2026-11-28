/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.c - eventos do events.json tocados no PlayState.
 */

#include "scenes/gameplay/events/PlayStateEventsInGameplay.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "raylib.h"
#include "yyjson.h"
#include "core/ModPath.h"
#include "Render.h"
#include "Game.h"
#include "GameData.h"
#include "core/Log.h"
#include "data/song/Song.h"
#include "data/song/SongEvents.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "rendering/shaders/ShaderFx.h"
#include "scenes/gameplay/characters/PlayStateAnimations.h"
#include "scenes/gameplay/visual/PlayStateCamera.h"
#include "scenes/api/FontLoader.h"

#define SCREEN_W 1280.0f
#include "scenes/gameplay/events/PlayStateParallax.h"
#include "scenes/gameplay/events/PlayStateCameraMove.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#define SCREEN_H 720.0f
#define HEALTH_BAR_NAME "Health Bar"
#define SHOW_IMAGE_NAME "Show Image"
#define HIDE_IMAGE_NAME "Hide Image"
#define CHANGE_CHAR_NAME "Change Character"
#define CAMERA_PARALLAX_NAME "Camera Parallax"
#define CAMERA_MOVE_NAME "Camera Move"
#define LETTERBOX_NAME "Letterbox"
#define MUSIC_CREDIT_NAME "Music Credit"
#define PLAY_SOUND_NAME "Play Sound"
#define CREDIT_DEFAULT_MS 4000.0
#define CREDIT_SLIDE_FRAC 0.12  /* fracao da duracao gasta entrando e saindo */
#define LETTERBOX_MAX_FRAC 0.5f  /* cada barra no maximo metade da tela */
#define IMG_MAX 8  /* imagens distintas por musica */
#define SND_MAX 16 /* sons distintos (Play Sound) por musica */
#define HEY_DEFAULT_SECS 0.6f
#define ZOOM_BUMP_MAX 1.35f      /* Psych: Add Camera Zoom so com a camera abaixo disso */

/* Psych: v1/v2 em texto livre. {nome, descricao, ..., rotulos, ..., textValues = 1, exemplos} */
#define PSYCH_EV(nm, desc, l1, l2, h1, h2) \
    {.name = nm, .description = desc, .label1 = l1, .label2 = l2, .textValues = 1, .hint1 = h1, .hint2 = h2}

const PlayStateEventType playStateEventTypes[] = {
    {"Fade In",       "black screen -> image (duration ms, final alpha %)", 1000, 0,   0,   "time", "fade",     NULL,       0},
    {"Fade Out",      "image -> black screen (duration ms, final alpha %)", 1000, 100, 0,   "time", "fade",     NULL,       0},
    {SHOW_IMAGE_NAME, "shows image (x, y from center in px; scale %)", 0, 0, 100, "x (px):", "y (px):", "scale %:", 1},
    {HIDE_IMAGE_NAME, "hides the shown image (no values)", 0, 0, 0, NULL, NULL, NULL, 0},
    {"Shader TV",     "old TV: lines, vignette, grain", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Glitch", "glitch: slices, RGB split, blocks", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Blackness", "black hole: pulls to center, darkens", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Grayscale", "everything black and white (intensity % = how much color is gone; animation has no effect)", 100, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Bloom",  "brightness: bright pixels get a halo (intensity % = strength and radius)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Space",  "space: colorful nebula + stars falling and twinkling (intensity % = strength, animation % = speed)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Pulse",  "pulse: thump-thump beat in rhythm (zoom + flash + border; animation % = BPM: 100 = 120)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Distortion", "distortion: heat waves + lens curvature (intensity % = strength, animation % = speed)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Chromatic", "chromatic aberration: splits red and blue from center (intensity % = strength, animation % = pulse)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Dissolve", "dissolve: pieces of the screen vanish by noise, orange border (intensity % = how much vanished, animation % = speed)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Depth", "depth: parallax by brightness + fog in background (intensity % = strength, animation % = camera sway)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Vignette", "black edges: darkens screen corners (intensity % = how dark, animation % = border pulse)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {HEALTH_BAR_NAME, "health bar and icons: 1 = show, 0 = hide", 1, 0, 0, "enabled (1/0):", NULL, NULL, 0},
    {"Shader Wave",   "stage background ripple (not counted in the 3 shaders; intensity % = strength, animation % = speed)", 50, 0, 0, "intensity %:", "animation %:", NULL, 0, 1},
    {"Shader Off",    "turns off the screen shader and background ripple", 0, 0, 0, NULL, NULL, NULL, 0, 1},
    {"Camera Parallax", "3D camera: pushes away (zoom out) and sways in depth (0 turns off)", 100, 0, 0, "intensity %:", "speed %:", NULL, 0, 0},
    {CAMERA_MOVE_NAME, "camera: zoom % (100 = normal), x and y in world px (smooth transition)", 100, 0, 0, "zoom %:", "x (px):", "y (px):", 0, 0},
    {CHANGE_CHAR_NAME, "changes character: 0=opponent 1=bf 2=gf (character name)", 0, 0, 0, "target (0/1/2):", NULL, NULL, 0, 0, 1},
    {LETTERBOX_NAME, "black film bars on top and bottom (duration ms, screen height %; 0 removes)", 1000, 12, 0, "time", "height %:", NULL, 0},
    {MUSIC_CREDIT_NAME, "title card with song name and creator (duration ms; text = creator name)", 4000, 0, 0, "time (ms):", NULL, NULL, 0, 0, 0, 1},
    /* Psych (padrao) */
    PSYCH_EV("Hey!", "BF does hey and GF cheers (who: bf, gf or empty = both; seconds, default 0.6)",
             "who:", "seconds:", "bf / gf / empty", "0.6"),
    PSYCH_EV("Set GF Speed", "GF dances every N beats (1 = normal, 2 = half)", "beats:", NULL, "1", NULL),
    PSYCH_EV("Add Camera Zoom", "zoom pulse (game default 0.015; HUD default 0.03)", "game:", "HUD:", "0.015", "0.03"),
    PSYCH_EV("Play Animation", "plays a character animation (name, e.g.: hey; who: dad, bf or gf)",
             "animation:", "who:", "hey", "dad / bf / gf"),
    PSYCH_EV("Camera Follow Pos", "locks camera at world x, y (both empty = back to following who sings)",
             "x:", "y:", "empty = release", "empty = release"),
    PSYCH_EV("Alt Idle Animation", "alternate idle (who: dad, bf or gf; suffix e.g.: -alt, empty = normal)",
             "who:", "suffix:", "dad / bf / gf", "-alt"),
    PSYCH_EV("Screen Shake", "screen shake: 'duration, intensity' (game and HUD)", "game:", "HUD:", "0.5, 0.01", "0.5, 0.005"),
    PSYCH_EV("Change Scroll Speed", "note speed (multiplier, 1 = normal; transition seconds)",
             "mult:", "seconds:", "1", "0"),
    PSYCH_EV("Set Property", "changes a variable (defaultCamZoom, cameraSpeed, camZooming, camHUD.visible, bf.alpha...)",
             "variable:", "value:", "defaultCamZoom", "0.9"),
    PSYCH_EV(PLAY_SOUND_NAME, "plays sounds/<name>.ogg (volume 0..1, default 1)", "sound:", "volume:", "sound name", "1"),
    /* JS Engine */
    PSYCH_EV("Flash Camera", "screen flash that fades (seconds, default 1; hex color, default FFFFFF)",
             "seconds:", "color:", "1", "FFFFFF"),
    PSYCH_EV("Set Camera Zoom", "default camera zoom (e.g.: 1.1; transition seconds, 0 = instant)",
             "zoom:", "seconds:", "1.1", "0"),
    PSYCH_EV("Credits Popup", "title card with title and creator (empty title = folder name)", "title:", "creator:",
             "title", "creator"),
    PSYCH_EV("Camera Bopping", "camera bop every N beats (default 4; 0 turns off) and intensity (default 1)",
             "beats:", "intensity:", "4", "1"),
};
const int playStateEventTypeCount = (int)(sizeof(playStateEventTypes) / sizeof(playStateEventTypes[0]));

/* Eventos de stage da Psych (dependem de sprites do stage): reconhecidos e ignorados. */
static const char* const ignoredEvents[] = {
    "Dadbattle Spotlight", "Philly Glow", "Kill Henchmen", "BG Freaks Expression", "Trigger BG Ghouls",
};
/* Outros nomes aceitos fora do picker. */
static const char* const extraEvents[] = {"Enable Camera Bop", "Disable Camera Bop"};

typedef struct {
    double timeMs;
    int seq;        /* ordem de leitura: desempata eventos no mesmo tempo */
    char name[48];
    double v1;      /* fade: duracao (ms) | imagem: x | Psych: value1 como numero */
    double v2;      /* fade: alpha final (%) | imagem: y | Psych: value2 como numero */
    double v3;      /* imagem: escala (%) */
    char has1;      /* value1 e um numero valido (Psych: vazio = padrao do evento) */
    char has2;
    char hasS1;     /* tem a posicao 4 (formato do engine) */
    char str1[64];  /* value1 como texto (Psych) */
    char str2[64];  /* value2 como texto (Psych) */
    char img[64];   /* imagem: nome em images/<img>.png | Change Character: nome do personagem */
} PsEvent;

typedef struct {
    char name[64];
    Texture2D tex;
    char ok;        /* textura carregada */
} PsImage;

typedef struct {
    char name[64];
    Sound snd;
    char ok;
} PsSound;

/* Transicao linear de um valor no tempo da musica (fade, letterbox, scroll, zoom). */
typedef struct {
    char on;
    double startMs;
    double durMs;
    float from;
    float to;
} Tween;

static PsEvent* evs;
static int evCount;
static int evCap;
static int nextIdx;    /* proximo evento a disparar */
static char seeking;   /* SeekTo: pula efeitos instantaneos (som, tremor, pulso, animacao) */
static float curAlpha; /* alpha do overlay preto (0..1) neste instante */

static Tween lbTween;    /* Letterbox: altura de cada barra (fracao da tela) */
static float curBar;     /* altura atual de cada barra (fracao da tela) neste instante */
static Tween fadeTween;  /* Fade In / Fade Out: alpha do overlay preto */
static Tween scrollTween;   /* Change Scroll Speed: multiplicador */
static float curScroll = 1.0f;
static Tween zoomTween;     /* Set Camera Zoom / defaultCamZoom: zoom padrao da camera */

/* Flash Camera: ultimo disparado. */
static char hasFlash;
static double flashStartMs;
static double flashDurMs;
static Color flashColor;
static float curFlash;

static char hudVisible = 1;
static char worldVisible = 1;

/* Ultimo Show Image disparado (-1 = nenhum). Derivado da ordem dos eventos, entao seek/restart batem. */
static PsImage imgs[IMG_MAX];
static int imgCount;
static int curImg = -1;
static double curX, curY, curScale;

static PsSound snds[SND_MAX];
static int sndCount;

/* Music Credit / Credits Popup: ultimo disparado. Visivel so enquanto curMs esta dentro da janela. */
static double curMs;
static char hasCredit;
static double creditStartMs;
static double creditDurMs;
static char folderTitle[64];   /* nome da pasta da musica (Load) */
static char creditTitle[64];   /* titulo mostrado (Credits Popup pode trocar) */
static char creditName[64];    /* criador */

/* ------------------------------------------------------------------ */
/* Valores                                                             */
/* ------------------------------------------------------------------ */

/* Numero no texto (Psych: Std.parseFloat). 0 = vazio ou nao numero. */
static char ParseNum(const char* s, double* out) {
    while (*s == ' ' || *s == '\t') s++;
    char* end = NULL;
    const double v = strtod(s, &end);
    if (end == s || !isfinite(v)) return 0;
    *out = v;
    return 1;
}

/* Minusculo e sem espacos nas pontas. */
static void LowerTrim(const char* in, char* out, size_t n) {
    while (*in == ' ' || *in == '\t') in++;
    size_t k = 0;
    for (; in[k] != 0 && k + 1 < n; k++) out[k] = (char)tolower((unsigned char)in[k]);
    while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '\t')) k--;
    out[k] = 0;
}

/* Play Animation / Alt Idle (Psych): bf/boyfriend ou 1 = BF, gf/girlfriend ou 2 = GF, resto = oponente.
 * Retorna o alvo do engine (0 = oponente, 1 = BF, 2 = GF). */
static int PsychTarget(const char* s) {
    char t[32];
    LowerTrim(s, t, sizeof(t));
    if (strcmp(t, "bf") == 0 || strcmp(t, "boyfriend") == 0) return 1;
    if (strcmp(t, "gf") == 0 || strcmp(t, "girlfriend") == 0) return 2;
    double v;
    if (ParseNum(t, &v)) {
        const long r = lround(v);
        if (r == 1) return 1;
        if (r == 2) return 2;
    }
    return 0;
}

/* Change Character (Psych): gf = 2, dad/opponent = oponente, numero 0 = BF, 1 = oponente, 2 = GF; resto = BF. */
static int PsychChangeTarget(const char* s) {
    char t[32];
    LowerTrim(s, t, sizeof(t));
    if (strcmp(t, "gf") == 0 || strcmp(t, "girlfriend") == 0) return 2;
    if (strcmp(t, "dad") == 0 || strcmp(t, "opponent") == 0) return 0;
    double v;
    if (ParseNum(t, &v)) {
        const long r = lround(v);
        if (r == 1) return 0;
        if (r == 2) return 2;
    }
    return 1;
}

/* "1, 0.05" -> duracao e intensidade (Screen Shake). */
static void ParsePair(const char* s, double* a, double* b) {
    *a = 0.0;
    *b = 0.0;
    (void)ParseNum(s, a);
    const char* comma = strchr(s, ',');
    if (comma != NULL) (void)ParseNum(comma + 1, b);
}

/* "FFFFFF", "#FFFFFF", "0xFFFFFF" ou "AARRGGBB". Vazio/invalido = branco. */
static Color ParseHexColor(const char* s) {
    while (*s == ' ' || *s == '#') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    char* end = NULL;
    const unsigned long v = strtoul(s, &end, 16);
    if (end == s) return WHITE;
    if (end - s > 6)
        return (Color){(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, (unsigned char)(v >> 24)};
    return (Color){(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, 255};
}

static float TweenAt(const Tween* t, double ms, float idle) {
    if (!t->on) return idle;
    if (t->durMs <= 0.0 || ms >= t->startMs + t->durMs) return t->to;
    if (ms <= t->startMs) return t->from;
    const double k = (ms - t->startMs) / t->durMs;
    return t->from + (t->to - t->from) * (float)k;
}

static void TweenStart(Tween* t, double startMs, double durMs, float from, float to) {
    *t = (Tween){1, startMs, durMs > 0.0 ? durMs : 0.0, from, to};
}

static int CmpEvent(const void* a, const void* b) {
    const PsEvent* x = a;
    const PsEvent* y = b;
    if (x->timeMs != y->timeMs) return (x->timeMs > y->timeMs) - (x->timeMs < y->timeMs);
    return (x->seq > y->seq) - (x->seq < y->seq);
}

static float PctToAlpha(double pct) {
    if (pct <= 0.0) return 0.0f;
    if (pct >= 100.0) return 1.0f;
    return (float)(pct / 100.0);
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

static int FindSnd(const char* name) {
    for (int i = 0; i < sndCount; i++)
        if (strcmp(snds[i].name, name) == 0) return i;
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
    else if (strcmp(name, "Shader Vignette") == 0) ShaderFx_Push(SHADERFX_VIGNETTE, amt, spd);
    else if (strcmp(name, "Shader Wave") == 0) ShaderFx_SetBackgroundWave(amt, spd);   /* fora da pilha */
    else if (strcmp(name, "Shader Off") == 0) ShaderFx_Reset();                        /* limpa pilha e ondulacao */
}

/* ------------------------------------------------------------------ */
/* Set Property (subconjunto das variaveis da Psych)                   */
/* ------------------------------------------------------------------ */

typedef enum {
    PROP_NONE, PROP_CAM_ZOOM, PROP_CAM_SPEED, PROP_CAM_ZOOMING, PROP_HUD, PROP_GAME,
    PROP_CHAR_ALPHA, PROP_HEALTH_BAR, PROP_FORCED_POS, PROP_GF_SPEED
} PropKind;

/* Retorna o tipo e, para personagem, o alvo (0 = oponente, 1 = BF, 2 = GF). */
static PropKind PropOf(const char* key, int* target) {
    static const struct { const char* key; PropKind kind; int target; } props[] = {
        {"defaultCamZoom", PROP_CAM_ZOOM, 0}, {"cameraSpeed", PROP_CAM_SPEED, 0},
        {"camZooming", PROP_CAM_ZOOMING, 0}, {"gfSpeed", PROP_GF_SPEED, 0},
        {"isCameraOnForcedPos", PROP_FORCED_POS, 0},
        {"camHUD.visible", PROP_HUD, 0}, {"camHUD.alpha", PROP_HUD, 0},
        {"camGame.visible", PROP_GAME, 0}, {"camGame.alpha", PROP_GAME, 0},
        {"healthBar.visible", PROP_HEALTH_BAR, 0}, {"healthBar.alpha", PROP_HEALTH_BAR, 0},
        {"iconP1.visible", PROP_HEALTH_BAR, 0}, {"iconP2.visible", PROP_HEALTH_BAR, 0},
        {"boyfriend.visible", PROP_CHAR_ALPHA, 1}, {"boyfriend.alpha", PROP_CHAR_ALPHA, 1},
        {"bf.visible", PROP_CHAR_ALPHA, 1}, {"bf.alpha", PROP_CHAR_ALPHA, 1},
        {"dad.visible", PROP_CHAR_ALPHA, 0}, {"dad.alpha", PROP_CHAR_ALPHA, 0},
        {"gf.visible", PROP_CHAR_ALPHA, 2}, {"gf.alpha", PROP_CHAR_ALPHA, 2},
        {"boyfriendGroup.visible", PROP_CHAR_ALPHA, 1}, {"boyfriendGroup.alpha", PROP_CHAR_ALPHA, 1},
        {"dadGroup.visible", PROP_CHAR_ALPHA, 0}, {"dadGroup.alpha", PROP_CHAR_ALPHA, 0},
        {"gfGroup.visible", PROP_CHAR_ALPHA, 2}, {"gfGroup.alpha", PROP_CHAR_ALPHA, 2},
    };
    char k[64];
    snprintf(k, sizeof(k), "%s", key);
    char* p = k;
    while (*p == ' ') p++;
    for (size_t n = strlen(p); n > 0 && p[n - 1] == ' '; n--) p[n - 1] = 0;
    for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
        if (strcmp(props[i].key, p) == 0) {
            *target = props[i].target;
            return props[i].kind;
        }
    }
    return PROP_NONE;
}

static void SetProperty(const PsEvent* e) {
    int target = 0;
    const PropKind kind = PropOf(e->str1, &target);
    char vs[16];
    LowerTrim(e->str2, vs, sizeof(vs));
    double v;
    if (strcmp(vs, "true") == 0) v = 1.0;
    else if (strcmp(vs, "false") == 0) v = 0.0;
    else if (!ParseNum(vs, &v)) return;

    switch (kind) {
    case PROP_CAM_ZOOM: TweenStart(&zoomTween, e->timeMs, 0.0, (float)v, (float)v); break;
    case PROP_CAM_SPEED: PlayStateCamera_SetSpeedOverride((float)v); break;
    case PROP_CAM_ZOOMING: PlayStateCamera_SetBopEnabled(v != 0.0); break;
    case PROP_GF_SPEED: PlayStateAnimations_SetGfSpeed((int)lround(v)); break;
    case PROP_HUD: hudVisible = v > 0.01; break;
    case PROP_GAME: worldVisible = v > 0.01; break;
    case PROP_HEALTH_BAR: PlayStateHealth_SetVisible(v > 0.01); break;
    case PROP_CHAR_ALPHA: PlayStateAnimations_SetAlpha(target, (float)v); break;
    case PROP_FORCED_POS: {
        const Vector2 at = PlayStateCamera_Target();
        PlayStateCamera_ForceFocus(v != 0.0, at.x, at.y);
        break;
    }
    case PROP_NONE: break;
    }
}

/* ------------------------------------------------------------------ */
/* Disparo                                                             */
/* ------------------------------------------------------------------ */

/* Eventos da Psych / JS Engine. Retorna 1 se o nome era um deles. */
static char FirePsych(const PsEvent* e) {
    const char* n = e->name;
    if (strcmp(n, "Hey!") == 0) {
        if (seeking) return 1;
        char who[16];
        LowerTrim(e->str1, who, sizeof(who));
        int value = 2;   /* os dois */
        if (strcmp(who, "bf") == 0 || strcmp(who, "boyfriend") == 0 || strcmp(who, "0") == 0) value = 0;
        else if (strcmp(who, "gf") == 0 || strcmp(who, "girlfriend") == 0 || strcmp(who, "1") == 0) value = 1;
        const float secs = (e->has2 && e->v2 > 0.0) ? (float)e->v2 : HEY_DEFAULT_SECS;
        if (value != 0 && !PlayStateAnimations_PlaySpecial(2, "cheer", secs))
            (void)PlayStateAnimations_PlaySpecial(0, "cheer", secs);   /* tutorial: a GF e o oponente */
        if (value != 1) (void)PlayStateAnimations_PlaySpecial(1, "hey", secs);
        return 1;
    }
    if (strcmp(n, "Set GF Speed") == 0) {
        PlayStateAnimations_SetGfSpeed((e->has1 && e->v1 >= 1.0) ? (int)lround(e->v1) : 1);
        return 1;
    }
    if (strcmp(n, "Add Camera Zoom") == 0) {
        if (seeking || PlayStateCamera_GameZoom() >= ZOOM_BUMP_MAX) return 1;
        const float game = e->has1 ? (float)e->v1 : 0.015f;
        const float hud = e->has2 ? (float)e->v2 : 0.03f;
        PlayStateCamera_Bump(hud * gameOptions.zoomFactorUI, game * gameOptions.zoomFactorGAME);
        return 1;
    }
    if (strcmp(n, "Play Animation") == 0) {
        if (!seeking) (void)PlayStateAnimations_PlaySpecial(PsychTarget(e->str2), e->str1, 0.0f);
        return 1;
    }
    if (strcmp(n, "Camera Follow Pos") == 0) {
        if (!e->has1 && !e->has2) PlayStateCamera_ForceFocus(0, 0.0f, 0.0f);
        else PlayStateCamera_ForceFocus(1, e->has1 ? (float)e->v1 : 0.0f, e->has2 ? (float)e->v2 : 0.0f);
        return 1;
    }
    if (strcmp(n, "Alt Idle Animation") == 0) {
        PlayStateAnimations_SetIdleSuffix(PsychTarget(e->str1), e->str2);
        return 1;
    }
    if (strcmp(n, "Screen Shake") == 0) {
        if (seeking) return 1;
        const char* vals[2] = {e->str1, e->str2};
        for (int i = 0; i < 2; i++) {
            double dur, intensity;
            ParsePair(vals[i], &dur, &intensity);
            if (dur > 0.0 && intensity != 0.0) PlayStateCamera_Shake((char)i, (float)intensity, (float)dur);
        }
        return 1;
    }
    if (strcmp(n, "Change Scroll Speed") == 0) {
        float to = e->has1 ? (float)e->v1 : 1.0f;
        if (to < 0.05f) to = 0.05f;
        const double durMs = e->has2 ? e->v2 * 1000.0 : 0.0;
        TweenStart(&scrollTween, e->timeMs, durMs, TweenAt(&scrollTween, e->timeMs, 1.0f), to);
        return 1;
    }
    if (strcmp(n, "Set Property") == 0) {
        SetProperty(e);
        return 1;
    }
    if (strcmp(n, PLAY_SOUND_NAME) == 0) {
        if (seeking) return 1;
        const int i = FindSnd(e->str1);
        if (i >= 0 && snds[i].ok) {
            const float vol = e->has2 ? (float)e->v2 : 1.0f;
            SetSoundVolume(snds[i].snd, vol * RayGame_MusicVolumeLevel());
            PlaySound(snds[i].snd);
        }
        return 1;
    }
    if (strcmp(n, "Flash Camera") == 0) {
        hasFlash = 1;
        flashStartMs = e->timeMs;
        flashDurMs = (e->has1 && e->v1 > 0.0 ? e->v1 : 1.0) * 1000.0;
        flashColor = ParseHexColor(e->str2);
        return 1;
    }
    if (strcmp(n, "Set Camera Zoom") == 0) {
        if (!e->has1 || e->v1 <= 0.0) return 1;
        const float from = zoomTween.on ? TweenAt(&zoomTween, e->timeMs, 1.0f) : PlayStateCamera_BaseZoom();
        TweenStart(&zoomTween, e->timeMs, e->has2 ? e->v2 * 1000.0 : 0.0, from, (float)e->v1);
        return 1;
    }
    if (strcmp(n, "Credits Popup") == 0) {
        hasCredit = 1;
        creditStartMs = e->timeMs;
        creditDurMs = CREDIT_DEFAULT_MS;
        snprintf(creditTitle, sizeof(creditTitle), "%s", e->str1[0] != 0 ? e->str1 : folderTitle);
        snprintf(creditName, sizeof(creditName), "%s", e->str2);
        return 1;
    }
    if (strcmp(n, "Camera Bopping") == 0) {
        PlayStateCamera_SetBop(e->has1 ? (int)lround(e->v1) : 4, e->has2 ? (float)e->v2 : 1.0f);
        return 1;
    }
    if (strcmp(n, "Enable Camera Bop") == 0 || strcmp(n, "Disable Camera Bop") == 0) {
        PlayStateCamera_SetBopEnabled(n[0] == 'E');
        return 1;
    }
    return 0;
}

static void Fire(const PsEvent* e) {
    if (strncmp(e->name, "Shader ", 7) == 0) {
        PlayStateEvents_ApplyShader(e->name, e->v1, e->v2);
        return;
    }
    if (FirePsych(e)) return;
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
        if (e->hasS1)                       /* engine: [nome, alvo 0/1/2, v2, v3, personagem] */
            PlayStateAnimations_ChangeCharacter((int)e->v1, e->img);
        else if (e->str2[0] != 0)           /* Psych: [nome, bf/dad/gf, personagem] */
            PlayStateAnimations_ChangeCharacter(PsychChangeTarget(e->str1), e->str2);
        return;
    }
    if (strcmp(e->name, MUSIC_CREDIT_NAME) == 0) {
        hasCredit = 1;
        creditStartMs = e->timeMs;
        creditDurMs = e->v1 > 0.0 ? e->v1 : CREDIT_DEFAULT_MS;
        snprintf(creditTitle, sizeof(creditTitle), "%s", folderTitle);
        snprintf(creditName, sizeof(creditName), "%s", e->img);
        return;
    }
    if (strcmp(e->name, HEALTH_BAR_NAME) == 0) {
        PlayStateHealth_SetVisible(e->v1 != 0.0);
        return;
    }
    if (strcmp(e->name, LETTERBOX_NAME) == 0) {
        /* anima a partir de onde esta */
        TweenStart(&lbTween, e->timeMs, e->v1, TweenAt(&lbTween, e->timeMs, 0.0f), PctToBar(e->v2));
        return;
    }

    float from;
    if (strcmp(e->name, "Fade In") == 0)
        from = 1.0f;                                     /* comeca preto e clareia */
    else if (strcmp(e->name, "Fade Out") == 0)
        from = TweenAt(&fadeTween, e->timeMs, 0.0f);     /* escurece a partir de onde esta */
    else
        return;                                          /* evento desconhecido: ignorado na gameplay */

    TweenStart(&fadeTween, e->timeMs, e->v1, from, PctToAlpha(e->v2));
}

/* ------------------------------------------------------------------ */
/* Carregar                                                            */
/* ------------------------------------------------------------------ */

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
static void LoadImages(const char* root) {
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

/* Psych (Paths.sound): <mod>/sounds/<nome>.ogg, depois assets/sounds e assets/shared/sounds. */
static int ResolveSound(const char* root, const char* name, char out[512]) {
    static const char* const exts[] = {"ogg", "wav", "mp3"};
    for (size_t e = 0; e < sizeof(exts) / sizeof(exts[0]); e++) {
        if (root[0] != '\0') {
            snprintf(out, 512, "%s/sounds/%s.%s", root, name, exts[e]);
            if (FileExists(out)) return 1;
        }
        snprintf(out, 512, "assets/sounds/%s.%s", name, exts[e]);
        if (FileExists(out)) return 1;
        snprintf(out, 512, "assets/shared/sounds/%s.%s", name, exts[e]);
        if (FileExists(out)) return 1;
    }
    return 0;
}

/* Sons dos Play Sound: carregados no Load para nao travar a musica no disparo. */
static void LoadSounds(const char* root) {
    for (int i = 0; i < evCount; i++) {
        if (strcmp(evs[i].name, PLAY_SOUND_NAME) != 0 || evs[i].str1[0] == '\0') continue;
        if (FindSnd(evs[i].str1) >= 0 || sndCount >= SND_MAX) continue;

        PsSound* slot = &snds[sndCount++];
        snprintf(slot->name, sizeof(slot->name), "%s", evs[i].str1);
        slot->ok = 0;

        char path[512];
        if (!ResolveSound(root, evs[i].str1, path)) {
            Log_WarnF("Play Sound: som nao encontrado: %s", evs[i].str1);
            continue;
        }
        slot->snd = LoadSound(path);
        slot->ok = IsSoundValid(slot->snd);
    }
}

static char InList(const char* name, const char* const* list, size_t count) {
    for (size_t i = 0; i < count; i++)
        if (strcmp(name, list[i]) == 0) return 1;
    return 0;
}

/* Um aviso por nome desconhecido / evento de stage ignorado / variavel do Set Property sem suporte. */
static void WarnUnsupported(void) {
    const char* warned[16];
    int nw = 0;
    for (int i = 0; i < evCount && nw < 16; i++) {
        const PsEvent* e = &evs[i];
        const char* what = NULL;
        const char* key = e->name;
        int target;
        if (InList(e->name, ignoredEvents, sizeof(ignoredEvents) / sizeof(ignoredEvents[0]))) {
            what = "evento de stage da Psych ignorado";
        } else if (strcmp(e->name, "Set Property") == 0) {
            if (PropOf(e->str1, &target) == PROP_NONE) {
                what = "Set Property: variavel sem suporte";
                key = e->str1;
            }
        } else {
            char known = InList(e->name, extraEvents, sizeof(extraEvents) / sizeof(extraEvents[0]));
            for (int k = 0; k < playStateEventTypeCount && !known; k++)
                known = (strcmp(playStateEventTypes[k].name, e->name) == 0);
            if (!known) what = "evento desconhecido ignorado";
        }
        if (what == NULL) continue;
        char seen = 0;
        for (int k = 0; k < nw && !seen; k++) seen = (strcmp(warned[k], key) == 0);
        if (seen) continue;
        warned[nw++] = key;
        Log_WarnF("%s: %s", what, key);
    }
}

/* user = quantos vieram do events.json (ou NULL): evento do chart igual a um deles e pulado
 * (o editor grava os do chart no events.json e o chart original pode continuar com eles). */
static char IsDuplicate(const PsEvent* e, int fileCount) {
    for (int i = 0; i < fileCount; i++) {
        const PsEvent* o = &evs[i];
        if (fabs(o->timeMs - e->timeMs) < 0.5 && strcmp(o->name, e->name) == 0 &&
            strcmp(o->str1, e->str1) == 0 && strcmp(o->str2, e->str2) == 0)
            return 1;
    }
    return 0;
}

static void OnEvent(void* user, double timeMs, const char* name, yyjson_val* const vals[4]) {
    if (evCount >= evCap) {
        const int ncap = evCap > 0 ? evCap * 2 : 64;
        PsEvent* n = realloc(evs, sizeof(PsEvent) * (size_t)ncap);
        if (n == NULL) return;
        evs = n;
        evCap = ncap;
    }
    PsEvent* e = &evs[evCount];
    memset(e, 0, sizeof(*e));
    e->timeMs = timeMs;
    e->seq = evCount++;
    snprintf(e->name, sizeof(e->name), "%s", name);
    SongEvents_ValStr(vals[0], e->str1, sizeof(e->str1));
    SongEvents_ValStr(vals[1], e->str2, sizeof(e->str2));
    e->has1 = ParseNum(e->str1, &e->v1);
    e->has2 = ParseNum(e->str2, &e->v2);
    /* Show Image: escala ausente = 100 %; Camera Move: y ausente = 0 px */
    char s3[32];
    SongEvents_ValStr(vals[2], s3, sizeof(s3));
    if (!ParseNum(s3, &e->v3)) e->v3 = (strcmp(e->name, CAMERA_MOVE_NAME) == 0) ? 0.0 : 100.0;
    e->hasS1 = (vals[3] != NULL);
    SongEvents_ValStr(vals[3], e->img, sizeof(e->img));
    if (user != NULL && IsDuplicate(e, *(const int*)user)) evCount--;
}

void PlayStateEvents_Unload(void) {
    for (int i = 0; i < imgCount; i++)
        if (imgs[i].ok) UnloadTexture(imgs[i].tex);
    imgCount = 0;
    curImg = -1;
    for (int i = 0; i < sndCount; i++)
        if (snds[i].ok) UnloadSound(snds[i].snd);
    sndCount = 0;

    free(evs);
    evs = NULL;
    evCount = 0;
    evCap = 0;
    PlayStateEvents_Reset();
}

void PlayStateEvents_Load(const char* songDir, const char* diff) {
    PlayStateEvents_Unload();

    /* Titulo do Music Credit: ultimo segmento do caminho da pasta da musica. */
    const char* base = strrchr(songDir, '/');
    const char* bslash = strrchr(songDir, '\\');
    if (bslash != NULL && (base == NULL || bslash > base)) base = bslash;
    snprintf(folderTitle, sizeof(folderTitle), "%s", base != NULL ? base + 1 : songDir);

    /* events.json da pasta + eventos do chart (Psych: "events" no chart, notas -1, data/<song>/events.json) */
    char path[1024];
    snprintf(path, sizeof(path), "%s/events.json", songDir);
    SongEvents_ReadFile(path, 0, OnEvent, NULL);
    int fileCount = evCount;
    if (Song_ChartPath(songDir, diff, path, sizeof(path)))
        SongEvents_ReadChartExtras(songDir, path, OnEvent, &fileCount);
    if (evCount == 0) return;

    qsort(evs, (size_t)evCount, sizeof(PsEvent), CmpEvent);
    char root[256] = "";
    (void)ModPath_Root(songDir, root, sizeof(root));
    LoadImages(root);
    LoadSounds(root);
    WarnUnsupported();
}

/* ------------------------------------------------------------------ */
/* Tempo                                                               */
/* ------------------------------------------------------------------ */

void PlayStateEvents_Reset(void) {
    nextIdx = 0;
    curAlpha = 0.0f;
    fadeTween.on = 0;
    lbTween.on = 0;
    curBar = 0.0f;
    scrollTween.on = 0;
    curScroll = 1.0f;
    zoomTween.on = 0;
    hasFlash = 0;
    curFlash = 0.0f;
    hudVisible = 1;
    worldVisible = 1;
    curImg = -1;
    hasCredit = 0;
    curMs = 0.0;
    ShaderFx_Reset();
    PlayStateHealth_SetVisible(1);   /* barra volta ao normal: o evento Health Bar a esconde de novo se precisar */
    PlayStateParallax_Reset();
    PlayStateCameraMove_Reset();
    PlayStateCamera_ResetEvents();   /* foco, tremor, zoom, velocidade e pulso da camera */
    PlayStateAnimations_ResetEvents();
}

/* Valores que dependem do tempo (fades, barras, scroll, zoom, flash). */
static void UpdateTimed(double ms) {
    curAlpha = TweenAt(&fadeTween, ms, 0.0f);
    curBar = TweenAt(&lbTween, ms, 0.0f);
    curScroll = TweenAt(&scrollTween, ms, 1.0f);
    if (zoomTween.on) PlayStateCamera_SetZoomOverride(TweenAt(&zoomTween, ms, 1.0f));
    curFlash = 0.0f;
    if (hasFlash && flashDurMs > 0.0 && ms >= flashStartMs && ms < flashStartMs + flashDurMs)
        curFlash = 1.0f - (float)((ms - flashStartMs) / flashDurMs);
}

static void FireUntil(double ms) {
    curMs = ms;
    while (nextIdx < evCount && evs[nextIdx].timeMs <= ms) {
        Fire(&evs[nextIdx]);
        nextIdx++;
    }
    UpdateTimed(ms);
}

void PlayStateEvents_SeekTo(double seconds) {
    PlayStateEvents_Reset();
    seeking = 1;
    FireUntil(seconds * 1000.0);
    seeking = 0;
}

void PlayStateEvents_Update(double seconds) {
    FireUntil(seconds * 1000.0);
}

float PlayStateEvents_ScrollMult(void) {
    return curScroll;
}

char PlayStateEvents_HudVisible(void) {
    return hudVisible;
}

char PlayStateEvents_WorldVisible(void) {
    return worldVisible;
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

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
    if (creditName[0] != 0) {
        DrawTextEx(mainFont, "by",(Vector2){tx, ty + 70.0f}, 26.0f, 1.0f, subCol);
        DrawTextEx(mainFont, creditName, (Vector2){tx, ty + 100.0f}, 36.0f, 1.0f, titleCol);
    }
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

    /* Flash Camera: por cima de tudo, some ate o fim da duracao. */
    if (curFlash > 0.001f) {
        const Color c = flashColor;
        DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H,
                      (Color){c.r, c.g, c.b, (unsigned char)((float)c.a * curFlash)});
    }
}

void PlayStateEvents_DrawLetterbox(void) {
    /* Barras pretas por cima do mundo, mas por baixo do HUD/notas para nao cobri-las. */
    if (curBar > 0.001f) {
        const int bh = (int)(curBar * SCREEN_H + 0.5f);
        DrawRectangle(0, 0, (int)SCREEN_W, bh, BLACK);
        DrawRectangle(0, (int)SCREEN_H - bh, (int)SCREEN_W, bh, BLACK);
    }
}
