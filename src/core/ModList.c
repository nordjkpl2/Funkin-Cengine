/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ModList.c - lista de mods e quais estao ativos.
 */

#include "core/ModList.h"
#include "core/ModPath.h"
#include "GameData.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[MOD_NAME_MAX];
    char root[256];
} ModInfo;

static ModInfo mods[MOD_LIST_MAX];
static int modCount;
static bool scanned;

static void Add(const char* name, const char* root)
{
    if (modCount >= MOD_LIST_MAX || strlen(name) >= MOD_NAME_MAX || strlen(root) >= sizeof(mods[0].root))
        return;
    strcpy(mods[modCount].name, name);
    strcpy(mods[modCount].root, root);
    modCount++;
}

void ModList_Rescan(void)
{
    modCount = 0;
    scanned = true;
    if (!DirectoryExists("assets/mods"))
        return;

    if (DirectoryExists("assets/mods/images") || DirectoryExists("assets/mods/characters"))
        Add("(direto)", "assets/mods");

    FilePathList list = LoadDirectoryFiles("assets/mods");
    for (unsigned int i = 0; i < list.count; i++) {
        const char* p = list.paths[i];
        if (!DirectoryExists(p))
            continue;
        const char* s1 = strrchr(p, '/');
        const char* s2 = strrchr(p, '\\');
        const char* b = (s1 > s2) ? s1 : s2;
        b = (b != NULL) ? b + 1 : p;
        if (b[0] == '\0' || ModPath_IsLayoutFolder(b))
            continue;
        char root[256];
        snprintf(root, sizeof(root), "assets/mods/%s", b);
        Add(b, root);
    }
    UnloadDirectoryFiles(list);
}

static void EnsureScanned(void)
{
    if (!scanned)
        ModList_Rescan();
}

int ModList_Count(void)
{
    EnsureScanned();
    return modCount;
}

const char* ModList_Name(int index)
{
    EnsureScanned();
    return (index >= 0 && index < modCount) ? mods[index].name : "";
}

const char* ModList_Root(int index)
{
    EnsureScanned();
    return (index >= 0 && index < modCount) ? mods[index].root : "";
}

static int DisabledIndex(const char* name)
{
    for (int i = 0; i < gameDisabledModCount; i++)
        if (strcmp(gameDisabledMods[i], name) == 0)
            return i;
    return -1;
}

bool ModList_Enabled(int index)
{
    EnsureScanned();
    if (index < 0 || index >= modCount)
        return false;
    return DisabledIndex(mods[index].name) < 0;
}

void ModList_SetEnabled(int index, bool enabled)
{
    EnsureScanned();
    if (index < 0 || index >= modCount)
        return;
    const int at = DisabledIndex(mods[index].name);
    if (enabled && at >= 0) {
        memmove(gameDisabledMods[at], gameDisabledMods[at + 1],
                (size_t)(gameDisabledModCount - at - 1) * MOD_NAME_MAX);
        gameDisabledModCount--;
    } else if (!enabled && at < 0 && gameDisabledModCount < MOD_DISABLED_MAX) {
        strcpy(gameDisabledMods[gameDisabledModCount++], mods[index].name);
    } else {
        return;
    }
    GameData_Flush();
}

static bool Exists(const char* root, const char* fmt, const char* name)
{
    char rel[300];
    char full[600];
    snprintf(rel, sizeof(rel), fmt, name);
    return ModPath_FindIn(root, rel, full, sizeof(full));
}

bool ModList_FindCharacterRoot(const char* name, char* out, size_t capacity)
{
    if (out == NULL || capacity == 0u)
        return false;
    out[0] = '\0';
    if (name == NULL || name[0] == '\0')
        return false;

    EnsureScanned();
    for (int i = 0; i < modCount; i++) {
        if (DisabledIndex(mods[i].name) >= 0)
            continue;
        const char* root = mods[i].root;
        if (Exists(root, "images/characters/%s.animset", name) ||
            Exists(root, "images/characters/%s.xml", name) ||
            Exists(root, "characters/%s.json", name)) {
            if (strlen(root) >= capacity)
                return false;
            strcpy(out, root);
            return true;
        }
    }
    return false;
}
