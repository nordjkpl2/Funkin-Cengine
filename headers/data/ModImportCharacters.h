/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportCharacters.h - Declaracoes publicas do modulo ModImportCharacters.
 */

#pragma once
#ifndef MODIMPORTCHARACTERS_H
#define MODIMPORTCHARACTERS_H

#include "data/ModImportInternal.h"

void MI_CopyCollectedIcons(MI_ImportContext *context);

int MI_CopyStagesRecursive(const char *sourceDir, const char *destinationDir,
                                  ModImportStats *stats, int depth);

int MI_CopyWeeksRecursive(const char *sourceDir, const char *destinationDir,
                                 MI_ImportContext *context, int depth);

int MI_ImportCharacters(const char *srcMod, const char *dstMod,
                               MI_ImportContext *context);

int MI_ImportSongs(const char *srcMod, const char *dstMod,
                          MI_ImportContext *context);

#endif /* MODIMPORTCHARACTERS_H */
