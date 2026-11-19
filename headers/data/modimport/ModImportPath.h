/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportPath.h - Declaracoes publicas do modulo ModImportPath.
 */

#pragma once
#ifndef MODIMPORTPATH_H
#define MODIMPORTPATH_H

#include "data/modimport/ModImportInternal.h"

const char *MI_BaseName(const char *path);

int MI_CaseEqual(const char *a, const char *b);

int MI_EndsWithI(const char *text, const char *suffix);

int MI_IsSafeComponent(const char *name);

void MI_SanitizeModName(char *dst, size_t dstSize, const char *src);

void MI_CopyString(char *dst, size_t dstSize, const char *src);

int MI_Join(char *out, size_t outSize, const char *a, const char *b);

int MI_EnsureDirectory(const char *path);

int MI_EnsureParent(const char *filePath);

#endif /* MODIMPORTPATH_H */
