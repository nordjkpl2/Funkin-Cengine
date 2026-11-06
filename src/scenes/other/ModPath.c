/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ModPath.c - descobre a raiz do mod a partir da pasta da musica.
 */

#include "scenes/ModPath.h"
#include <string.h>

#define MODS_PREFIX "assets/mods/"
#define MODS_PREFIX_LEN (sizeof(MODS_PREFIX) - 1u)

/* Pastas que, no primeiro nivel de assets/mods, indicam mod importado direto. */
static bool IsTopLevelFolder(const char* name, size_t length)
{
    static const char* const folders[] = {
        "songs", "weeks", "data", "stages", "images", "characters", "scripts", "sounds", "music"
    };
    for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
        if (strlen(folders[i]) == length && memcmp(folders[i], name, length) == 0)
            return true;
    }
    return false;
}

bool ModPath_Root(const char* restrict songDir, char* restrict out, size_t capacity)
{
    if (out == NULL || capacity == 0u) return false;
    out[0] = '\0';
    if (songDir == NULL || songDir[0] == '\0') return false;

    char path[512];
    size_t length = strlen(songDir);
    if (length >= sizeof(path)) length = sizeof(path) - 1u;
    for (size_t i = 0; i < length; i++)
        path[i] = songDir[i] == '\\' ? '/' : songDir[i];
    path[length] = '\0';

    const char* mods = strstr(path, MODS_PREFIX);
    if (mods == NULL) return false;

    const char* name = mods + MODS_PREFIX_LEN;
    const char* slash = strchr(name, '/');
    const size_t nameLength = slash != NULL ? (size_t)(slash - name) : strlen(name);
    if (nameLength == 0u) return false;

    /* Direto: a raiz e a propria assets/mods. Senao: assets/mods/<mod>. */
    const char* end = IsTopLevelFolder(name, nameLength) ? name - 1 : name + nameLength;
    const size_t rootLength = (size_t)(end - path);
    if (rootLength >= capacity) return false;

    memcpy(out, path, rootLength);
    out[rootLength] = '\0';
    return true;
}
