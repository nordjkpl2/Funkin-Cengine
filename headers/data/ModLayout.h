/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModLayout.h - Declaracoes publicas do modulo ModLayout.
 */

#pragma once
#ifndef MODLAYOUT_H
#define MODLAYOUT_H

#include "data/ModImportInternal.h"

int MI_HasModLayout(const char *root);

int MI_IsLayoutDirName(const char *name);

int MI_IsModsRoot(const char *root);

int MI_IsNameMods(const char *name);

#endif /* MODLAYOUT_H */
