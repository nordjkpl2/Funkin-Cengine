/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportFormat.c - Deteccao do formato do mod de origem, a partir do conteudo dos .json.
 *
 * Sinais (o primeiro que bater decide, na ordem V-Slice > Psych > FNF):
 *   V-Slice: chart com objeto "scrollSpeed" + "notes" por dificuldade, ou metadata com "playData".
 *   Psych:   pack.json na raiz, ou week com "weekCharacters".
 *   FNF:     chart com "song" (ou raiz) contendo "notes" como lista de secoes.
 */

#include "std.h"
#include "data/modimport/ModImportFormat.h"
#include "data/modimport/ModImportFiles.h"
#include "data/modimport/ModImportPath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"
#include "raylib.h"

#define MI_CLASS_MAX_BYTES (32L * 1024L * 1024L)  /* mesmo teto dos charts V-Slice e do analyze */
#define MI_CLASS_SLOTS     4096                   /* potencia de 2; cheio = so le, sem guardar */
#define MI_FORMAT_MAX_FILES 400   /* teto de arquivos lidos na deteccao: o import nao pode travar */
#define MI_FORMAT_MAX_DEPTH 5     /* profundidade: charts V-Slice podem ficar em data/songs/<musica>/charts/ */

typedef struct MI_ScanState {
    int vslice;
    int psychMarker;
    int fnfChart;
    int filesRead;
} MI_ScanState;

const char *MI_FormatName(MI_Format format)
{
    switch (format) {
    case MI_FMT_PSYCH: return "Psych";
    case MI_FMT_FNF: return "FNF (base)";
    case MI_FMT_VSLICE: return "V-Slice";
    default: return "";
    }
}

/* Pastas que nunca tem chart: pular poupa tempo (muitos arquivos de imagem/audio). */
static int MI_IsAssetDir(const char *name)
{
    static const char *const skip[] = {
        "images", "sounds", "music", "characters", "videos", "fonts", "shaders", "icons", "stages"
    };
    for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); ++i) {
        if (MI_CaseEqual(name, skip[i])) return 1;
    }
    return 0;
}

void MI_WalkJson(const char *dir, int depth, int maxDepth, MI_JsonVisitFn fn, void *user)
{
    FilePathList entries;
    if (fn == NULL || dir == NULL || depth > maxDepth || !DirectoryExists(dir)) return;
    entries = MI_ListEntries(dir);
    for (unsigned i = 0; i < entries.count; ++i) {
        const char *path = entries.paths[i];
        const char *base = MI_BaseName(path);
        if (MI_IsDirectoryPath(path)) {
            if (!MI_IsAssetDir(base)) MI_WalkJson(path, depth + 1, maxDepth, fn, user);
        } else if (MI_EndsWithI(base, ".json")) {
            fn(path, user);
        }
    }
    UnloadDirectoryFiles(entries);
}

/* ---- classificacao com cache ---- */

typedef struct MI_ClassSlot {
    char *path;     /* NULL = vazio */
    MI_JsonKind kind;
} MI_ClassSlot;

static MI_ClassSlot *classSlots = NULL;
static int classUsed = 0;

static unsigned MI_ClassHash(const char *s)
{
    unsigned h = 2166136261u;   /* FNV-1a */
    for (; *s != '\0'; ++s) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

/* Le e classifica (sem cache). Mesma ordem de sinais de sempre: V-Slice > Psych > FNF. */
static MI_JsonKind MI_ClassifyUncached(const char *path)
{
    long size = 0;
    char *buffer;
    yyjson_doc *doc;
    yyjson_val *root;
    MI_JsonKind kind = MI_JK_NONE;

    buffer = MI_ReadFile(path, &size, MI_CLASS_MAX_BYTES);
    if (buffer == NULL) return MI_JK_NONE;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return MI_JK_NONE;

    root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) {
        if (yyjson_is_obj(yyjson_obj_get(root, "scrollSpeed")) &&
            yyjson_is_obj(yyjson_obj_get(root, "notes"))) {
            kind = MI_JK_VS_CHART;
        } else if (yyjson_is_obj(yyjson_obj_get(root, "playData"))) {
            kind = MI_JK_VS_META;
        } else if (yyjson_obj_get(root, "weekCharacters") != NULL) {
            kind = MI_JK_PSYCH_WEEK;
        } else {
            yyjson_val *songObject = yyjson_obj_get(root, "song");
            if (!yyjson_is_obj(songObject)) songObject = root;
            if (yyjson_is_arr(yyjson_obj_get(songObject, "notes"))) kind = MI_JK_FNF_CHART;
        }
    }
    yyjson_doc_free(doc);
    return kind;
}

MI_JsonKind MI_ClassifyJson(const char *path)
{
    unsigned i;
    unsigned probes;
    int found = 0;
    MI_JsonKind kind;

    if (path == NULL) return MI_JK_NONE;
    if (classSlots == NULL) classSlots = (MI_ClassSlot *)calloc(MI_CLASS_SLOTS, sizeof(*classSlots));
    if (classSlots == NULL) return MI_ClassifyUncached(path);

    i = MI_ClassHash(path) & (MI_CLASS_SLOTS - 1u);
    for (probes = 0; probes < MI_CLASS_SLOTS; ++probes) {
        if (classSlots[i].path == NULL) break;
        if (strcmp(classSlots[i].path, path) == 0) return classSlots[i].kind;
        i = (i + 1u) & (MI_CLASS_SLOTS - 1u);
    }
    found = probes < MI_CLASS_SLOTS && classSlots[i].path == NULL && classUsed < MI_CLASS_SLOTS - 1;

    kind = MI_ClassifyUncached(path);
    if (found) {
        size_t len = strlen(path);
        char *copy = (char *)malloc(len + 1u);
        if (copy != NULL) {
            memcpy(copy, path, len + 1u);
            classSlots[i].path = copy;
            classSlots[i].kind = kind;
            ++classUsed;
        }
    }
    return kind;
}

void MI_ClassifyCacheClear(void)
{
    if (classSlots != NULL) {
        for (int i = 0; i < MI_CLASS_SLOTS; ++i) free(classSlots[i].path);
        free(classSlots);
        classSlots = NULL;
    }
    classUsed = 0;
}

static void MI_ScanFile(const char *path, void *user)
{
    MI_ScanState *state = (MI_ScanState *)user;

    if (state->filesRead >= MI_FORMAT_MAX_FILES) return;
    ++state->filesRead;
    switch (MI_ClassifyJson(path)) {
    case MI_JK_VS_CHART:
    case MI_JK_VS_META:   state->vslice = 1; break;
    case MI_JK_PSYCH_WEEK: state->psychMarker = 1; break;
    case MI_JK_FNF_CHART: state->fnfChart = 1; break;
    default: break;
    }
}

MI_Format MI_DetectFormat(const char *srcMod)
{
    MI_ScanState state;
    char packPath[MI_PATH_MAX];

    memset(&state, 0, sizeof(state));
    if (srcMod == NULL || !DirectoryExists(srcMod)) return MI_FMT_UNKNOWN;

    MI_WalkJson(srcMod, 0, MI_FORMAT_MAX_DEPTH, MI_ScanFile, &state);
    if (state.vslice) return MI_FMT_VSLICE;
    if (state.psychMarker) return MI_FMT_PSYCH;
    if (MI_Join(packPath, sizeof(packPath), srcMod, "pack.json") && FileExists(packPath))
        return MI_FMT_PSYCH;
    if (state.fnfChart) return MI_FMT_FNF;
    return MI_FMT_UNKNOWN;
}
