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
#include <string.h>

#include "yyjson.h"
#include "raylib.h"

#define MI_FORMAT_MAX_JSON (4L * 1024L * 1024L)
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

static void MI_ScanFile(const char *path, void *user)
{
    MI_ScanState *state = (MI_ScanState *)user;
    long size = 0;
    char *buffer;
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *songObject;

    if (state->filesRead >= MI_FORMAT_MAX_FILES) return;
    ++state->filesRead;
    buffer = MI_ReadFile(path, &size, MI_FORMAT_MAX_JSON);
    if (buffer == NULL) return;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return;

    root = yyjson_doc_get_root(doc);
    if (yyjson_is_obj(root)) {
        yyjson_val *scroll = yyjson_obj_get(root, "scrollSpeed");
        yyjson_val *notes = yyjson_obj_get(root, "notes");
        if (yyjson_is_obj(scroll) && yyjson_is_obj(notes)) {
            state->vslice = 1;
        } else if (yyjson_is_obj(yyjson_obj_get(root, "playData"))) {
            state->vslice = 1;
        } else if (yyjson_obj_get(root, "weekCharacters") != NULL) {
            state->psychMarker = 1;
        } else {
            songObject = yyjson_obj_get(root, "song");
            if (!yyjson_is_obj(songObject)) songObject = root;
            if (yyjson_is_arr(yyjson_obj_get(songObject, "notes"))) state->fnfChart = 1;
        }
    }
    yyjson_doc_free(doc);
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
