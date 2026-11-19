/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportFiles.h - Declaracoes publicas do modulo ModImportFiles.
 */

#pragma once
#ifndef MODIMPORTFILES_H
#define MODIMPORTFILES_H

#include "data/modimport/ModImportInternal.h"

char MI_CopyFile(const char *source, const char *destination, long long *bytes);

char MI_CopyFileNow(const char *source, const char *destination, long long *bytes);

int MI_CopyTree(const char *source, const char *destination,
                       long long *bytes, int *copiedFiles, int depth);

long long MI_CountTreeBytes(const char *path);

void MI_Drain(long long *bytes);

long MI_FileSize(const char *path);

void MI_GetStem(const char *path, char *out, size_t outSize);

int MI_IsDirectoryPath(const char *path);

FilePathList MI_ListDirectories(const char *path);

FilePathList MI_ListEntries(const char *path);

char *MI_ReadFile(const char *path, long *outSize, long maxSize);

int MI_RemoveTree(const char *path, int depth, int *removedFiles);

void MI_StatsAdd(ModImportStats *dst, const ModImportStats *src);

#endif /* MODIMPORTFILES_H */
