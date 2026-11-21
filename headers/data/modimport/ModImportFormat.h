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

/* Classificacao de um .json (uma leitura por arquivo na importacao toda; ver MI_ClassifyJson). */
typedef enum MI_JsonKind {
    MI_JK_NONE = 0,
    MI_JK_VS_CHART,     /* V-Slice: "scrollSpeed" + "notes" por dificuldade */
    MI_JK_VS_META,      /* V-Slice: metadata com "playData" */
    MI_JK_PSYCH_WEEK,   /* week com "weekCharacters" */
    MI_JK_FNF_CHART     /* chart FNF: "notes" (lista de secoes) em "song" ou na raiz */
} MI_JsonKind;

/* Classifica o .json uma vez por importacao: o resultado fica num cache por caminho.
 * Chamar so na thread do import. MI_ClassifyCacheClear libera o cache. */
MI_JsonKind MI_ClassifyJson(const char *path);
void MI_ClassifyCacheClear(void);

/* Callback do walker: recebe o caminho de cada .json (sem pastas de imagem/audio). */
typedef void (*MI_JsonVisitFn)(const char *path, void *user);

/* Percorre dir ate maxDepth niveis chamando fn para cada .json. Pastas de assets sao puladas. */
void MI_WalkJson(const char *dir, int depth, int maxDepth, MI_JsonVisitFn fn, void *user);

/* Olha o conteudo de algumas dezenas de .json do mod e decide o formato. */
MI_Format MI_DetectFormat(const char *srcMod);

#endif /* MODIMPORTFORMAT_H */
