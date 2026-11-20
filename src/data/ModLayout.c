/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModLayout.c - Deteccao do layout de mod (raiz de mods e pastas do jogo).
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/ModLayout.h"
#include "data/modimport/ModImportPath.h"


#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int MI_IsModsRoot(const char *root)
{
    size_t len;
    size_t modsStart;
    size_t assetsEnd;
    size_t assetsStart;
    if (root == NULL || root[0] == '\0' || strstr(root, "..") != NULL) return 0;
    len = strlen(root);
    while (len > 0 && (root[len - 1] == '/' || root[len - 1] == '\\')) --len;
    if (len < 11u) return 0;

    modsStart = len - 4u;
    if (modsStart == 0 || (root[modsStart - 1] != '/' && root[modsStart - 1] != '\\')) return 0;
    if (memcmp(root + modsStart, "mods", 4) != 0 &&
        memcmp(root + modsStart, "MODS", 4) != 0) {
        for (size_t i = 0; i < 4u; ++i) {
            unsigned char c = (unsigned char)root[modsStart + i];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
            if (c != (unsigned char)"mods"[i]) return 0;
        }
    }

    assetsEnd = modsStart - 1u;
    assetsStart = assetsEnd;
    while (assetsStart > 0 && root[assetsStart - 1u] != '/' &&
           root[assetsStart - 1u] != '\\') --assetsStart;
    if (assetsEnd - assetsStart != 6u) return 0;
    for (size_t i = 0; i < 6u; ++i) {
        unsigned char c = (unsigned char)root[assetsStart + i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
        if (c != (unsigned char)"assets"[i]) return 0;
    }
    return 1;
}

/* A pasta selecionada pode ser um mod individual ou a pasta-container "mods".
 * Quando ela ja contem a estrutura do jogo, ela propria e o mod (assets/mods).
 */
int MI_HasModLayout(const char *root)
{
    static const char *const folders[] = {
        "songs", "weeks", "stages", "characters", "images", "data"
    };
    char path[MI_PATH_MAX];
    if (root == NULL || !DirectoryExists(root)) return 0;
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i) {
        if (MI_Join(path, sizeof(path), root, folders[i]) && DirectoryExists(path))
            return 1;
    }
    return 0;
}

int MI_IsNameMods(const char *name)
{
    return MI_CaseEqual(name, "mods");
}

/* Pastas do layout do jogo: dentro de assets/mods elas NAO sao mods separados. */
int MI_IsLayoutDirName(const char *name)
{
    static const char *const folders[] = {
        "songs", "weeks", "stages", "characters", "images", "data",
        "sounds", "music", "scripts"
    };
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); ++i) {
        if (MI_CaseEqual(name, folders[i])) return 1;
    }
    return 0;
}
