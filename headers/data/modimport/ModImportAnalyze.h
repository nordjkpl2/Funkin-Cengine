/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportAnalyze.h - Custo de rodar um chart importado (notas, pico de densidade, sustains).
 */

#pragma once
#ifndef MODIMPORTANALYZE_H
#define MODIMPORTANALYZE_H

#include "data/modimport/ModImportInternal.h"

typedef struct MI_ChartCost {
    int notes;
    int sustains;
    int peakNps;      /* maior numero de notas em uma janela de 1 s */
    int level;        /* 0 baixo, 1 medio, 2 alto */
} MI_ChartCost;

/* Le um chart no formato do engine (Psych: "song.notes" com "sectionNotes").
 * Retorna 0 se o arquivo nao puder ser lido. */
char MI_AnalyzeChart(const char *chartPath, MI_ChartCost *out);

/* Mesma analise, sobre um documento ja parseado (raiz do chart). Evita reler o arquivo. */
char MI_AnalyzeChartRoot(yyjson_val *root, MI_ChartCost *out);

/* Custo a partir dos tempos das notas (ordem qualquer; o vetor e ordenado in-place).
 * sustains = notas com comprimento > 0. */
void MI_CostFromTimes(float *times, int count, int sustains, MI_ChartCost *out);

/* Acumula o custo de um chart nas estatisticas do import (soma e pior nivel). */
void MI_AddChartCost(ModImportStats *stats, const MI_ChartCost *cost);

const char *MI_CostLevelName(int level);

/* Texto curto para a mensagem do import (" | formato ... | custo ..."). "" se nada foi analisado. */
void MI_FormatAnalysisLine(char *out, size_t outSize, const ModImportStats *stats);

#endif /* MODIMPORTANALYZE_H */
