/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportParse.h - Declaracoes publicas do modulo ModImportParse.
 */

#pragma once
#ifndef MODIMPORTPARSE_H
#define MODIMPORTPARSE_H

#include "data/ModImportInternal.h"

void MI_AddName(MI_NameList *list, const char *name);

int MI_ParseCharacterJson(const char *jsonPath, const char *fallbackName,
                                 MI_CharInfo *out);

void MI_ParseChartActors(const char *chartPath, MI_NameList *names);

void MI_ParseWeekIcons(const char *weekPath, MI_NameList *names);

int MI_WriteAnimset(const MI_CharInfo *info, const char *xmlPath,
                           const char *animsetPath);

#endif /* MODIMPORTPARSE_H */
