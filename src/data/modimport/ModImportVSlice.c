/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportVSlice.c - Converte charts V-Slice (notes por dificuldade, tempo em ms) para o
 * formato de chart que o engine le (secoes de 16 passos com "sectionNotes").
 *
 * Premissas (sem chart V-Slice real para validar aqui):
 *   - lanes V-Slice: 0-3 = oponente, 4-7 = jogador (trocadas para o lado do mustHitSection).
 *   - BPM: primeiro "timeChanges" do metadata (mudancas de BPM no meio da musica ainda nao sao
 *     convertidas; o tempo das secoes usa um BPM so).
 *   - Personagens e stage vem do metadata "playData"; ausente = bf / dad / gf / stage.
 */

#include "std.h"
#include "data/modimport/ModImportVSlice.h"
#include "data/modimport/ModImportFormat.h"
#include "data/modimport/ModImportAnalyze.h"
#include "data/modimport/ModImportFiles.h"
#include "data/modimport/ModImportPath.h"
#include "data/modimport/ModImportParse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"
#include "raylib.h"

#define MI_VS_MAX_BYTES     (32L * 1024L * 1024L)
#define MI_VS_MAX_CHARTS    64
#define MI_VS_MAX_SECTIONS  100000   /* mesmo teto do SongJson */
#define MI_VS_DEFAULT_BPM   100.0
#define MI_VS_DEPTH         5

typedef struct MI_VsMeta {
    double bpm;
    char player1[MI_NAME_MAX];
    char player2[MI_NAME_MAX];
    char gf[MI_NAME_MAX];
    char stage[MI_NAME_MAX];
} MI_VsMeta;

typedef struct MI_VsCollect {
    char paths[MI_VS_MAX_CHARTS][MI_PATH_MAX];
    int count;
    /* Metadata da ultima pasta lida: os charts de uma mesma pasta nao releem o metadata. */
    char metaDir[MI_PATH_MAX];
    MI_VsMeta meta;
    int metaReady;
} MI_VsCollect;

static void MI_VsMetaDefaults(MI_VsMeta *meta)
{
    meta->bpm = MI_VS_DEFAULT_BPM;
    MI_CopyString(meta->player1, sizeof(meta->player1), "bf");
    MI_CopyString(meta->player2, sizeof(meta->player2), "dad");
    MI_CopyString(meta->gf, sizeof(meta->gf), "gf");
    MI_CopyString(meta->stage, sizeof(meta->stage), "stage");
}

static yyjson_doc *MI_VsReadDoc(const char *path)
{
    long size = 0;
    char *buffer = MI_ReadFile(path, &size, MI_VS_MAX_BYTES);
    yyjson_doc *doc;
    if (buffer == NULL) return NULL;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    return doc;
}

/* Nome do campo string, ou fallback quando ausente/invalido. */
static void MI_VsString(char *dst, size_t dstSize, yyjson_val *value, const char *fallback)
{
    MI_CopyString(dst, dstSize, yyjson_is_str(value) ? yyjson_get_str(value) : fallback);
}

/* Usa o cache de classificacao: cada .json e lido no maximo uma vez por importacao. */
int MI_VSliceFileKind(const char *path)
{
    switch (MI_ClassifyJson(path)) {
    case MI_JK_VS_CHART: return 1;
    case MI_JK_VS_META:  return 2;
    default:             return 0;
    }
}

/* Le o metadata (primeiro "*.json" com playData) de uma pasta. Sem metadata: padroes. */
static void MI_VsReadMeta(const char *dir, MI_VsMeta *meta)
{
    FilePathList entries;
    MI_VsMetaDefaults(meta);
    entries = MI_ListEntries(dir);
    for (unsigned i = 0; i < entries.count; ++i) {
        yyjson_doc *doc;
        yyjson_val *root;
        yyjson_val *playData;
        yyjson_val *characters;
        yyjson_val *timeChanges;
        yyjson_val *first;
        if (MI_IsDirectoryPath(entries.paths[i]) || !MI_EndsWithI(MI_BaseName(entries.paths[i]), ".json")) continue;
        if (MI_VSliceFileKind(entries.paths[i]) != 2) continue;

        doc = MI_VsReadDoc(entries.paths[i]);
        if (doc == NULL) continue;
        root = yyjson_doc_get_root(doc);
        timeChanges = yyjson_obj_get(root, "timeChanges");
        first = yyjson_is_arr(timeChanges) ? yyjson_arr_get(timeChanges, 0) : NULL;
        if (yyjson_is_obj(first) && yyjson_is_num(yyjson_obj_get(first, "bpm")) &&
            yyjson_get_num(yyjson_obj_get(first, "bpm")) >= 10.0) {
            meta->bpm = yyjson_get_num(yyjson_obj_get(first, "bpm"));
        }
        playData = yyjson_obj_get(root, "playData");
        if (yyjson_is_obj(playData)) {
            MI_VsString(meta->stage, sizeof(meta->stage), yyjson_obj_get(playData, "stage"), meta->stage);
            characters = yyjson_obj_get(playData, "characters");
            if (yyjson_is_obj(characters)) {
                MI_VsString(meta->player1, sizeof(meta->player1), yyjson_obj_get(characters, "player"), meta->player1);
                MI_VsString(meta->player2, sizeof(meta->player2), yyjson_obj_get(characters, "opponent"), meta->player2);
                MI_VsString(meta->gf, sizeof(meta->gf), yyjson_obj_get(characters, "girlfriend"), meta->gf);
            }
        }
        yyjson_doc_free(doc);
        break;
    }
    UnloadDirectoryFiles(entries);
}

static void MI_StripSuffix(char *name, const char *suffix)
{
    const size_t nameLen = strlen(name);
    const size_t suffixLen = strlen(suffix);
    if (nameLen > suffixLen && strcmp(name + nameLen - suffixLen, suffix) == 0)
        name[nameLen - suffixLen] = '\0';
}

/* Pasta que contem o arquivo. Caminhos podem misturar '/' e '\' (o walker devolve assim),
 * entao corta no separador final de verdade, como MI_BaseName. */
static void MI_VsParentDir(const char *path, char *out, size_t outSize)
{
    const char *base = MI_BaseName(path);
    size_t len = base > path ? (size_t)(base - path) - 1u : 0u;
    if (outSize == 0) return;
    if (len >= outSize) len = outSize - 1u;
    memcpy(out, path, len);
    out[len] = '\0';
}

/* Nome da musica: pasta do chart (se tiver nome de musica) ou o arquivo sem "-chart"/"-metadata". */
static int MI_VsSongName(const char *chartPath, char *out, size_t outSize)
{
    char dirPath[MI_PATH_MAX];
    char stem[MI_NAME_MAX];
    const char *parent;

    MI_VsParentDir(chartPath, dirPath, sizeof(dirPath));
    parent = MI_BaseName(dirPath);

    MI_GetStem(chartPath, stem, sizeof(stem));
    MI_StripSuffix(stem, "-chart");
    MI_StripSuffix(stem, "-metadata");

    if (parent[0] == '\0' || MI_CaseEqual(parent, "charts") || MI_CaseEqual(parent, "data") ||
        MI_CaseEqual(parent, "songs") || MI_CaseEqual(parent, "preload"))
        MI_CopyString(out, outSize, stem);
    else
        MI_CopyString(out, outSize, parent);
    return MI_IsSafeComponent(out) && out[0] != '\0';
}

/* Converte uma dificuldade: notes (lista {t, d, l}) -> chart do engine em outPath. */
static int MI_VsWriteDifficulty(yyjson_val *notes, double bpm, double speed,
                                const MI_VsMeta *meta, const char *song,
                                const char *outPath, MI_ImportContext *context)
{
    const double stepMs = 60000.0 / bpm / 4.0;
    const double sectionMs = 16.0 * stepMs;
    double maxTime = 0.0;
    size_t idx;
    size_t max;
    yyjson_val *note;
    size_t sectionCount;
    yyjson_mut_doc *doc = NULL;
    yyjson_mut_val *root;
    yyjson_mut_val *songObject;
    yyjson_mut_val *sections;
    yyjson_mut_val **noteArrays = NULL;
    float *times = NULL;      /* tempos das notas gravadas: custo sem reler o arquivo */
    int sustains = 0;
    int kept = 0;
    int ok = 0;

    if (!yyjson_is_arr(notes) || yyjson_arr_size(notes) == 0) return 0;

    yyjson_arr_foreach(notes, idx, max, note) {
        const double t = yyjson_get_num(yyjson_obj_get(note, "t"));
        if (t > maxTime) maxTime = t;
    }
    sectionCount = (size_t)(maxTime / sectionMs) + 1u;
    if (sectionCount > MI_VS_MAX_SECTIONS) return 0;

    doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) return 0;
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    songObject = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "song", songObject);
    yyjson_mut_obj_add_str(doc, songObject, "song", song);
    yyjson_mut_obj_add_real(doc, songObject, "bpm", bpm);
    yyjson_mut_obj_add_real(doc, songObject, "speed", (double)(int)(speed * 10.0 + 0.5) / 10.0);
    yyjson_mut_obj_add_str(doc, songObject, "player1", meta->player1);
    yyjson_mut_obj_add_str(doc, songObject, "player2", meta->player2);
    yyjson_mut_obj_add_str(doc, songObject, "gfVersion", meta->gf);
    yyjson_mut_obj_add_str(doc, songObject, "stage", meta->stage);
    sections = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, songObject, "notes", sections);

    noteArrays = (yyjson_mut_val **)calloc(sectionCount, sizeof(*noteArrays));
    if (noteArrays == NULL) goto cleanup;
    times = (float *)malloc(sizeof(float) * yyjson_arr_size(notes));  /* NULL: so pula o custo */
    for (size_t s = 0; s < sectionCount; ++s) {
        yyjson_mut_val *section = yyjson_mut_obj(doc);
        noteArrays[s] = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, section, "sectionNotes", noteArrays[s]);
        yyjson_mut_obj_add_int(doc, section, "lengthInSteps", 16);
        yyjson_mut_obj_add_bool(doc, section, "mustHitSection", true);
        yyjson_mut_obj_add_real(doc, section, "bpm", bpm);
        yyjson_mut_obj_add_bool(doc, section, "changeBPM", false);
        yyjson_mut_obj_add_bool(doc, section, "altAnim", false);
        yyjson_mut_obj_add_bool(doc, section, "gfSection", false);
        yyjson_mut_obj_add_int(doc, section, "sectionBeats", 4);
        yyjson_mut_arr_add_val(sections, section);
    }

    yyjson_arr_foreach(notes, idx, max, note) {
        const double t = yyjson_get_num(yyjson_obj_get(note, "t"));
        const int lane = (int)yyjson_get_num(yyjson_obj_get(note, "d"));
        const double len = yyjson_get_num(yyjson_obj_get(note, "l"));
        size_t section;
        yyjson_mut_val *triple;
        int psychLane;
        if (lane < 0 || lane > 7 || t < 0.0) continue;
        /* V-Slice 0-3 = oponente, 4-7 = jogador; Psych com mustHit: 0-3 = jogador. */
        psychLane = lane < 4 ? lane + 4 : lane - 4;
        section = (size_t)(t / sectionMs);
        if (section >= sectionCount) section = sectionCount - 1u;

        triple = yyjson_mut_arr(doc);
        yyjson_mut_arr_add_real(doc, triple, t);
        yyjson_mut_arr_add_int(doc, triple, psychLane);
        yyjson_mut_arr_add_real(doc, triple, len > 0.0 ? len : 0.0);
        yyjson_mut_arr_add_val(noteArrays[section], triple);
        if (times != NULL) times[kept] = (float)t;
        if (len > 0.0) ++sustains;
        ++kept;
    }
    if (kept == 0) goto cleanup;

    if (!MI_EnsureParent(outPath)) goto cleanup;
    if (!yyjson_mut_write_file(outPath, doc, 0, NULL, NULL)) goto cleanup;

    if (times != NULL) {
        MI_ChartCost cost;
        MI_CostFromTimes(times, kept, sustains, &cost);
        MI_AddChartCost(context->stats, &cost);
    }
    ++context->stats->charts;
    ok = 1;

cleanup:
    free(times);
    free(noteArrays);
    yyjson_mut_doc_free(doc);
    return ok;
}

/* Converte todas as dificuldades de um chart V-Slice (um arquivo = varias dificuldades). */
static void MI_VsConvertChart(const char *chartPath, const char *dstMod,
                              MI_ImportContext *context, MI_VsCollect *collect)
{
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *scroll;
    yyjson_val *notesByDiff;
    yyjson_obj_iter iter;
    yyjson_val *key;
    char song[MI_NAME_MAX];
    char dirPath[MI_PATH_MAX];
    char songsRoot[MI_PATH_MAX];
    char songDir[MI_PATH_MAX];
    MI_VsMeta *meta = &collect->meta;
    int wrote = 0;

    if (!MI_VsSongName(chartPath, song, sizeof(song))) return;
    doc = MI_VsReadDoc(chartPath);
    if (doc == NULL) return;
    root = yyjson_doc_get_root(doc);
    scroll = yyjson_obj_get(root, "scrollSpeed");
    notesByDiff = yyjson_obj_get(root, "notes");
    if (!yyjson_is_obj(scroll) || !yyjson_is_obj(notesByDiff)) goto done;

    MI_VsParentDir(chartPath, dirPath, sizeof(dirPath));
    if (!collect->metaReady || strcmp(collect->metaDir, dirPath) != 0) {
        MI_VsReadMeta(dirPath, meta);
        MI_CopyString(collect->metaDir, sizeof(collect->metaDir), dirPath);
        collect->metaReady = 1;
    }

    /* Os personagens do metadata viram icones do import (mesmo caminho dos charts do engine). */
    MI_AddName(&context->iconNames, meta->player1);
    MI_AddName(&context->iconNames, meta->player2);
    MI_AddName(&context->iconNames, meta->gf);

    if (!MI_Join(songsRoot, sizeof(songsRoot), dstMod, "songs") ||
        !MI_Join(songDir, sizeof(songDir), songsRoot, song) || !MI_EnsureDirectory(songDir)) goto done;

    yyjson_obj_iter_init(notesByDiff, &iter);
    while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
        yyjson_val *notes = yyjson_obj_iter_get_val(key);
        const char *diff = yyjson_get_str(key);
        double speed = yyjson_get_num(yyjson_obj_get(scroll, diff));
        char fileName[MI_NAME_MAX + 32];
        char outPath[MI_PATH_MAX];
        if (speed < 0.1) speed = 1.0;
        /* "normal" vira <musica>.json (padrao do FNF); as outras, <musica>-<dificuldade>.json. */
        if (MI_CaseEqual(diff, "normal")) snprintf(fileName, sizeof(fileName), "%s.json", song);
        else snprintf(fileName, sizeof(fileName), "%s-%s.json", song, diff);
        if (!MI_IsSafeComponent(fileName) || !MI_Join(outPath, sizeof(outPath), songDir, fileName)) continue;
        if (MI_VsWriteDifficulty(notes, meta->bpm, speed, meta, song, outPath, context)) wrote = 1;
    }
    if (wrote) ++context->stats->songs;

done:
    yyjson_doc_free(doc);
}

static void MI_VsCollectVisit(const char *path, void *user)
{
    MI_VsCollect *collect = (MI_VsCollect *)user;
    if (collect->count >= MI_VS_MAX_CHARTS) return;
    if (MI_VSliceFileKind(path) != 1) return;
    MI_CopyString(collect->paths[collect->count], sizeof(collect->paths[0]), path);
    ++collect->count;
}

int MI_ImportVSliceCharts(const char *srcMod, const char *dstMod, MI_ImportContext *context)
{
    MI_VsCollect *collect;
    if (srcMod == NULL || dstMod == NULL || context == NULL || context->stats == NULL) return 0;

    collect = (MI_VsCollect *)calloc(1, sizeof(*collect));
    if (collect == NULL) return 0;
    MI_WalkJson(srcMod, 0, MI_VS_DEPTH, MI_VsCollectVisit, collect);
    for (int i = 0; i < collect->count; ++i)
        MI_VsConvertChart(collect->paths[i], dstMod, context, collect);
    free(collect);
    return 1;
}
