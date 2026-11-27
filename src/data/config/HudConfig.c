/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * HudConfig.c - carrega, valida e grava assets/hud.json.
 * Campos ausentes ou fora da faixa caem no padrao; o arquivo nunca quebra o jogo.
 */
#include "data/config/HudConfig.h"
#include "yyjson.h"

#include <stdbool.h>

#define HUD_PATH "assets/hud.json"

static HudConfig current;
static char loaded;

/* chave no JSON e nome no editor, na ordem de HudElementId */
static const char* const keys[HUD_ELEMENTS] = {"health", "time", "score", "title", "botplay"};
static const char* const names[HUD_ELEMENTS] = {"Health Bar", "Time", "Score", "Song Title", "Botplay"};

void HudConfig_Defaults(HudConfig* out) {
    for (int i = 0; i < HUD_ELEMENTS; i++)
        out->el[i] = (HudElement){.offsetX = 0, .offsetY = 0, .scalePct = 100, .visible = 1};
    out->barWidthPct = 100;
}

const char* HudConfig_Name(int id) {
    return (id >= 0 && id < HUD_ELEMENTS) ? names[id] : "";
}

static int Clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int IntOr(yyjson_val* obj, const char* key, int fallback, int lo, int hi) {
    yyjson_val* v = obj ? yyjson_obj_get(obj, key) : NULL;
    if (!yyjson_is_num(v)) return fallback;
    return Clamp((int)yyjson_get_num(v), lo, hi);
}

static void ReadFromJson(yyjson_val* root, HudConfig* c) {
    for (int i = 0; i < HUD_ELEMENTS; i++) {
        yyjson_val* o = yyjson_obj_get(root, keys[i]);
        HudElement* e = &c->el[i];
        if (!yyjson_is_obj(o)) continue;
        e->offsetX = IntOr(o, "offsetX", e->offsetX, -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
        e->offsetY = IntOr(o, "offsetY", e->offsetY, -HUD_OFFSET_MAX, HUD_OFFSET_MAX);
        e->scalePct = IntOr(o, "scalePct", e->scalePct, HUD_SCALE_MIN, HUD_SCALE_MAX);
        yyjson_val* vis = yyjson_obj_get(o, "visible");
        if (yyjson_is_bool(vis)) e->visible = yyjson_get_bool(vis) ? 1 : 0;
    }
    c->barWidthPct = IntOr(yyjson_obj_get(root, "health"), "widthPct", c->barWidthPct, HUD_WIDTH_MIN, HUD_WIDTH_MAX);
}

static void EnsureLoaded(void) {
    if (loaded) return;
    loaded = 1;
    HudConfig_Defaults(&current);

    yyjson_doc* doc = yyjson_read_file(HUD_PATH, 0, NULL, NULL);
    if (doc == NULL) return;
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) ReadFromJson(root, &current);
    yyjson_doc_free(doc);
}

const HudConfig* HudConfig_Get(void) {
    EnsureLoaded();
    return &current;
}

void HudConfig_Set(const HudConfig* cfg) {
    EnsureLoaded();
    if (cfg != NULL) current = *cfg;
}

char HudConfig_Save(void) {
    EnsureLoaded();
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    for (int i = 0; i < HUD_ELEMENTS; i++) {
        const HudElement* e = &current.el[i];
        yyjson_mut_val* o = yyjson_mut_obj_add_obj(doc, root, keys[i]);
        yyjson_mut_obj_add_int(doc, o, "offsetX", e->offsetX);
        yyjson_mut_obj_add_int(doc, o, "offsetY", e->offsetY);
        yyjson_mut_obj_add_int(doc, o, "scalePct", e->scalePct);
        yyjson_mut_obj_add_bool(doc, o, "visible", e->visible != 0);
        if (i == HUD_EL_HEALTH) yyjson_mut_obj_add_int(doc, o, "widthPct", current.barWidthPct);
    }

    const bool ok = yyjson_mut_write_file(HUD_PATH, doc, YYJSON_WRITE_PRETTY, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok ? 1 : 0;
}
