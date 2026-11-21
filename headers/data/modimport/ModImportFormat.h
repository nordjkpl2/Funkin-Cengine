/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportFormat.h - Deteccao do formato do mod de origem (Psych, FNF base, V-Slice).
 */

#pragma once
#ifndef MODIMPORTFORMAT_H
#define MODIMPORTFORMAT_H

#include "data/modimport/ModImportInternal.h"

typedef enum MI_Format {
    MI_FMT_UNKNOWN = 0,
    MI_FMT_PSYCH,   /* Psych Engine: pack.json ou weeks com "weekCharacters" */
    MI_FMT_FNF,     /* Friday Night Funkin' base: chart com "song" e "notes" em secoes */
    MI_FMT_VSLICE   /* V-Slice: chart com "scrollSpeed" e "notes" por dificuldade */
} MI_Format;

const char *MI_FormatName(MI_Format format);

/* Callback do walker: recebe o caminho de cada .json (sem pastas de imagem/audio). */
typedef void (*MI_JsonVisitFn)(const char *path, void *user);

/* Percorre dir ate maxDepth niveis chamando fn para cada .json. Pastas de assets sao puladas. */
void MI_WalkJson(const char *dir, int depth, int maxDepth, MI_JsonVisitFn fn, void *user);

/* Olha o conteudo de algumas dezenas de .json do mod e decide o formato. */
MI_Format MI_DetectFormat(const char *srcMod);

#endif /* MODIMPORTFORMAT_H */
