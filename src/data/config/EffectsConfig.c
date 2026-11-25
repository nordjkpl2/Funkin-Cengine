/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * EffectsConfig.c - carrega, valida e grava assets/effects.json.
 * Campos ausentes ou fora da faixa caem no padrao; o arquivo nunca quebra o jogo.
 */
#include "data/config/EffectsConfig.h"
#include "yyjson.h"

#include <stdio.h>
#include <string.h>

#define EFFECTS_PATH "assets/effects.json"

static EffectsConfig current;
static char loaded;

static const Color tintColors[EFFECTS_TINTS] = {
    {255, 255, 255, 255},  /* branco (padrao) */
    {255, 90, 90, 255},    /* vermelho */
    {90, 220, 255, 255},   /* ciano */
    {255, 230, 90, 255},   /* amarelo */
    {255, 120, 220, 255},  /* rosa */
    {140, 255, 140, 255},  /* verde */
};
static const char* const tintNames[EFFECTS_TINTS] = {"branco", "vermelho", "ciano", "amarelo", "rosa", "verde"};

void EffectsConfig_Defaults(EffectsConfig* out) {
    *out = (EffectsConfig){
        .splash = {.scalePct = 80, .alphaPct = 60, .fps = 24, .offsetX = 0, .offsetY = 0, .tint = 0},
        .camera = {.zoomPct = 100, .offsetX = 0, .offsetY = 0},
    };
    for (int i = 0; i < EFFECTS_RATINGS; i++)
        out->popups[i] = (EffectsPopup){.scalePct = 60, .offsetX = 0, .offsetY = 0};
}

static int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int IntOr(yyjson_val* obj, const char* key, int fallback, int lo, int hi) {
    yyjson_val* v = obj ? yyjson_obj_get(obj, key) : NULL;
    if (!yyjson_is_num(v)) return fallback;
    return Clamp((int)yyjson_get_num(v), lo, hi);
}

static void ReadFromJson(yyjson_val* root, EffectsConfig* c) {
    yyjson_val* s = yyjson_obj_get(root, "splash");
    c->splash.scalePct = IntOr(s, "scalePct", c->splash.scalePct, 10, 300);
    c->splash.alphaPct = IntOr(s, "alphaPct", c->splash.alphaPct, 0, 100);
    c->splash.fps = IntOr(s, "fps", c->splash.fps, 1, 60);
    c->splash.offsetX = IntOr(s, "offsetX", c->splash.offsetX, -1000, 1000);
    c->splash.offsetY = IntOr(s, "offsetY", c->splash.offsetY, -1000, 1000);
    c->splash.tint = IntOr(s, "tint", c->splash.tint, 0, EFFECTS_TINTS - 1);

    yyjson_val* popups = yyjson_obj_get(root, "popups");
    static const char* const names[EFFECTS_RATINGS] = {"shit", "bad", "good", "sick"};
    for (int i = 0; i < EFFECTS_RATINGS; i++) {
        yyjson_val* p = popups ? yyjson_obj_get(popups, names[i]) : NULL;
        c->popups[i].scalePct = IntOr(p, "scalePct", c->popups[i].scalePct, 10, 300);
        c->popups[i].offsetX = IntOr(p, "offsetX", c->popups[i].offsetX, -1000, 1000);
        c->popups[i].offsetY = IntOr(p, "offsetY", c->popups[i].offsetY, -1000, 1000);
    }

    yyjson_val* cam = yyjson_obj_get(root, "camera");
    c->camera.zoomPct = IntOr(cam, "zoomPct", c->camera.zoomPct, 50, 200);
    c->camera.offsetX = IntOr(cam, "offsetX", c->camera.offsetX, -1000, 1000);
    c->camera.offsetY = IntOr(cam, "offsetY", c->camera.offsetY, -1000, 1000);
}

static void EnsureLoaded(void) {
    if (loaded) return;
    loaded = 1;
    EffectsConfig_Defaults(&current);

    yyjson_doc* doc = yyjson_read_file(EFFECTS_PATH, 0, NULL, NULL);
    if (doc == NULL) return;
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) ReadFromJson(root, &current);
    yyjson_doc_free(doc);
}

const EffectsConfig* EffectsConfig_Get(void) {
    EnsureLoaded();
    return &current;
}

void EffectsConfig_Set(const EffectsConfig* cfg) {
    EnsureLoaded();
    if (cfg != NULL) current = *cfg;
}

static void AddInt(yyjson_mut_doc* doc, yyjson_mut_val* obj, const char* key, int v) {
    yyjson_mut_obj_add_int(doc, obj, key, v);
}

char EffectsConfig_Save(void) {
    EnsureLoaded();
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val* s = yyjson_mut_obj_add_obj(doc, root, "splash");
    AddInt(doc, s, "scalePct", current.splash.scalePct);
    AddInt(doc, s, "alphaPct", current.splash.alphaPct);
    AddInt(doc, s, "fps", current.splash.fps);
    AddInt(doc, s, "offsetX", current.splash.offsetX);
    AddInt(doc, s, "offsetY", current.splash.offsetY);
    AddInt(doc, s, "tint", current.splash.tint);

    yyjson_mut_val* popups = yyjson_mut_obj_add_obj(doc, root, "popups");
    static const char* const names[EFFECTS_RATINGS] = {"shit", "bad", "good", "sick"};
    for (int i = 0; i < EFFECTS_RATINGS; i++) {
        yyjson_mut_val* p = yyjson_mut_obj_add_obj(doc, popups, names[i]);
        AddInt(doc, p, "scalePct", current.popups[i].scalePct);
        AddInt(doc, p, "offsetX", current.popups[i].offsetX);
        AddInt(doc, p, "offsetY", current.popups[i].offsetY);
    }

    yyjson_mut_val* cam = yyjson_mut_obj_add_obj(doc, root, "camera");
    AddInt(doc, cam, "zoomPct", current.camera.zoomPct);
    AddInt(doc, cam, "offsetX", current.camera.offsetX);
    AddInt(doc, cam, "offsetY", current.camera.offsetY);

    const bool ok = yyjson_mut_write_file(EFFECTS_PATH, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok ? 1 : 0;
}

Color EffectsConfig_TintColor(int index) {
    if (index < 0 || index >= EFFECTS_TINTS) return WHITE;
    return tintColors[index];
}

const char* EffectsConfig_TintName(int index) {
    if (index < 0 || index >= EFFECTS_TINTS) return tintNames[0];
    return tintNames[index];
}
