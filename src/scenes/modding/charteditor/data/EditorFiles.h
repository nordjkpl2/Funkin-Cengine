/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: utilitarios de arquivo e nome (pasta, copia, nome seguro). */
#pragma once
#ifndef EDITOR_FILES_H
#define EDITOR_FILES_H

#include <stddef.h>

const char* EdDirName(const char* path); /* ultimo componente do caminho */
char EdCopyFile(const char* src, const char* dst);
void EdSanitizeName(char* dst, size_t dstSize, const char* src); /* so [A-Za-z0-9_-] */

#endif
