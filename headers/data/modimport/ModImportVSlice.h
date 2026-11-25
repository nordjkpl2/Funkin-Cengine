/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportVSlice.h - Conversao dos charts V-Slice para o formato de chart do engine.
 */

#pragma once
#ifndef MODIMPORTVSLICE_H
#define MODIMPORTVSLICE_H

#include "data/modimport/ModImportInternal.h"

/* 0 = nao e V-Slice, 1 = chart (notes por dificuldade), 2 = metadata (playData). */
int MI_VSliceFileKind(const char *path);

/* Procura charts V-Slice em srcMod e grava cada dificuldade em dstMod/songs/<musica>/.
 * Retorna 1 mesmo sem nenhum chart V-Slice (nao e erro). */
int MI_ImportVSliceCharts(const char *srcMod, const char *dstMod, MI_ImportContext *context);

/* Levels V-Slice (pasta data/levels, json) viram weeks/<nivel>.json do engine. */
int MI_ImportVSliceLevels(const char *srcMod, const char *dstMod, MI_ImportContext *context);

#endif /* MODIMPORTVSLICE_H */
