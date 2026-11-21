/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportAnalyze.c - Analise de custo dos charts: quantas notas, densidade de pico e sustains.
 *
 * Heuristica (nao e benchmark): o custo de tocar um chart cresce com a densidade de notas
 * (cada nota vira um objeto a desenhar/checar por frame) e com o total de notas.
 *   pico > 40 nps ou > 15000 notas  -> alto
 *   pico > 20 nps ou >  6000 notas  -> medio
 *   caso contrario                  -> baixo
 */

#include "std.h"
#include "data/modimport/ModImportAnalyze.h"
#include "data/modimport/ModImportFiles.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

#define MI_ANALYZE_MAX_BYTES (32L * 1024L * 1024L)
#define MI_NPS_WINDOW_MS     1000.0f

static int MI_LevelByPeak(int peakNps)
{
    if (peakNps > 40) return 2;
    if (peakNps > 20) return 1;
    return 0;
}

static int MI_LevelByNotes(int notes)
{
    if (notes > 15000) return 2;
    if (notes > 6000) return 1;
    return 0;
}

static int MI_CmpFloat(const void *a, const void *b)
{
    const float x = *(const float *)a;
    const float y = *(const float *)b;
    return (x > y) - (x < y);
}

/* Pico de notas em MI_NPS_WINDOW_MS: janela deslizante sobre os tempos ordenados (O(n)). */
static int MI_PeakNps(const float *times, int count)
{
    int peak = 0;
    int end = 0;
    for (int start = 0; start < count; ++start) {
        if (end < start) end = start;
        while (end < count && times[end] - times[start] < MI_NPS_WINDOW_MS) ++end;
        if (end - start > peak) peak = end - start;
    }
    return peak;
}

/* Acrescenta um tempo ao vetor (cresce por dobra). Retorna 0 se faltou memoria. */
static int MI_PushTime(float **times, int *count, int *cap, float value)
{
    if (*count == *cap) {
        const int newCap = *cap > 0 ? *cap * 2 : 1024;
        float *grown = (float *)realloc(*times, sizeof(float) * (size_t)newCap);
        if (grown == NULL) return 0;
        *times = grown;
        *cap = newCap;
    }
    (*times)[(*count)++] = value;
    return 1;
}

char MI_AnalyzeChart(const char *chartPath, MI_ChartCost *out)
{
    long size = 0;
    char *buffer;
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *songObject;
    yyjson_val *sections;
    float *times = NULL;
    int count = 0;
    int cap = 0;
    int sustains = 0;
    int ok = 0;
    size_t sectionIdx;
    size_t sectionMax;
    yyjson_val *section;

    if (out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    if (chartPath == NULL) return 0;

    buffer = MI_ReadFile(chartPath, &size, MI_ANALYZE_MAX_BYTES);
    if (buffer == NULL) return 0;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return 0;

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) goto done;
    songObject = yyjson_obj_get(root, "song");
    if (!yyjson_is_obj(songObject)) songObject = root;
    sections = yyjson_obj_get(songObject, "notes");
    if (!yyjson_is_arr(sections)) goto done;

    ok = 1;
    yyjson_arr_foreach(sections, sectionIdx, sectionMax, section) {
        yyjson_val *sectionNotes;
        size_t noteIdx;
        size_t noteMax;
        yyjson_val *note;
        if (!yyjson_is_obj(section)) continue;
        sectionNotes = yyjson_obj_get(section, "sectionNotes");
        if (!yyjson_is_arr(sectionNotes)) continue;
        yyjson_arr_foreach(sectionNotes, noteIdx, noteMax, note) {
            yyjson_val *timeValue;
            yyjson_val *lengthValue;
            if (!yyjson_is_arr(note) || yyjson_arr_size(note) < 2) continue;
            timeValue = yyjson_arr_get(note, 0);
            if (!yyjson_is_num(timeValue)) continue;
            if (!MI_PushTime(&times, &count, &cap, (float)yyjson_get_num(timeValue))) {
                ok = 0;
                goto done;
            }
            lengthValue = yyjson_arr_get(note, 2);
            if (yyjson_is_num(lengthValue) && yyjson_get_num(lengthValue) > 0.0) ++sustains;
        }
    }

    if (count > 1) qsort(times, (size_t)count, sizeof(float), MI_CmpFloat);
    out->notes = count;
    out->sustains = sustains;
    out->peakNps = MI_PeakNps(times, count);
    {
        const int byPeak = MI_LevelByPeak(out->peakNps);
        const int byNotes = MI_LevelByNotes(out->notes);
        out->level = byPeak > byNotes ? byPeak : byNotes;
    }

done:
    free(times);
    yyjson_doc_free(doc);
    return (char)ok;
}

void MI_AddChartCost(ModImportStats *stats, const MI_ChartCost *cost)
{
    if (stats == NULL || cost == NULL) return;
    if (stats->notes <= INT_MAX - cost->notes) stats->notes += cost->notes;
    if (stats->sustains <= INT_MAX - cost->sustains) stats->sustains += cost->sustains;
    if (cost->peakNps > stats->peakNps) stats->peakNps = cost->peakNps;
    if (cost->level > stats->costLevel) stats->costLevel = cost->level;
}

const char *MI_CostLevelName(int level)
{
    switch (level) {
    case 2: return "alto";
    case 1: return "medio";
    default: return "baixo";
    }
}

void MI_FormatAnalysisLine(char *out, size_t outSize, const ModImportStats *stats)
{
    if (out == NULL || outSize == 0) return;
    out[0] = '\0';
    if (stats == NULL || (stats->notes == 0 && stats->format[0] == '\0')) return;
    (void)snprintf(out, outSize,
        " | formato %s | %d notas (%d longas) | pico %d nps | custo %s",
        stats->format[0] != '\0' ? stats->format : "?",
        stats->notes, stats->sustains, stats->peakNps,
        MI_CostLevelName(stats->costLevel));
}
