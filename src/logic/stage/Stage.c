/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Stage.c - Busca de arquivos do stage (.lua / .json) e validacao de nome.
 */

#include "logic/stage/Stage.h"
#include "core/ModPath.h"

#include <stdio.h>
#include <string.h>

#define STAGE_PATH_MAX 300
#define STAGE_MOD_ROOT_MAX 256

static inline bool Stage_IsSafeName(const char *name)
{
    if (name == NULL || name[0] == '\0' || strstr(name, "..") != NULL)
        return false;

    for (const char *p = name; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\')
            return false;
    }
    return true;
}

/* Ordem de busca: mod/stages, mod/data/stages, assets/stages,
 * assets/data/stages, songDir. Serve para .lua e .json. */
static char Stage_FindFile(const char *stageName, const char *songDir,
                           const char *ext, char out[STAGE_PATH_MAX])
{
    char modRoot[STAGE_MOD_ROOT_MAX];
    char relative[STAGE_PATH_MAX];

    if (out == NULL)
        return 0;

    out[0] = '\0';

    if (!Stage_IsSafeName(stageName) || strcmp(stageName, "stage") == 0)
        return 0;

    if (!ModPath_Root(songDir, modRoot, sizeof(modRoot)))
        modRoot[0] = '\0';

    snprintf(relative, sizeof(relative), "stages/%s.%s", stageName, ext);
    if (ModPath_FindIn(modRoot, relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "data/stages/%s.%s", stageName, ext);
    if (ModPath_FindIn(modRoot, relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "stages/%s.%s", stageName, ext);
    if (ModPath_FindIn("assets", relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "data/stages/%s.%s", stageName, ext);
    if (ModPath_FindIn("assets", relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "%s.%s", stageName, ext);
    return ModPath_FindIn(songDir, relative, out, STAGE_PATH_MAX) ? 1 : 0;
}

char Stage_FindLua(const char *stageName, const char *songDir, char out[300])
{
    return Stage_FindFile(stageName, songDir, "lua", out);
}

char Stage_FindJson(const char *stageName, const char *songDir, char out[300])
{
    return Stage_FindFile(stageName, songDir, "json", out);
}
