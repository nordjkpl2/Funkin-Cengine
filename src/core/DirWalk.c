/* DirWalk.c - ver DirWalk.h. Sem raylib de proposito (windows.h conflita com os nomes do raylib). */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* lstat/readdir em -std=c2x estrito */
#endif

#include "core/DirWalk.h"

#include <stdio.h>
#include <string.h>

#define DW_PATH_MAX 1100
#define DW_NAME_MAX 520

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct DwIter {
    HANDLE handle;
    WIN32_FIND_DATAW data;
    int first;
} DwIter;

static int DwOpen(const char* path, DwIter* it) {
    char pattern[DW_PATH_MAX];
    wchar_t wide[DW_PATH_MAX];
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) >= (int)sizeof(pattern)) return 0;
    if (MultiByteToWideChar(CP_UTF8, 0, pattern, -1, wide, DW_PATH_MAX) <= 0) return 0;
    it->handle = FindFirstFileExW(wide, FindExInfoBasic, &it->data, FindExSearchNameMatch, NULL,
                                  FIND_FIRST_EX_LARGE_FETCH);
    it->first = 1;
    return it->handle != INVALID_HANDLE_VALUE;
}

/* 1 = entrada valida; 0 = acabou. Ignora "." e ".." e pastas que sao links/juncoes. */
static int DwNext(DwIter* it, const char* dirPath, char* name, int cap, int* isDir, long long* size) {
    (void)dirPath;
    for (;;) {
        if (it->first) it->first = 0;
        else if (!FindNextFileW(it->handle, &it->data)) return 0;

        const wchar_t* n = it->data.cFileName;
        if (n[0] == L'.' && (n[1] == 0 || (n[1] == L'.' && n[2] == 0))) continue;

        const DWORD attr = it->data.dwFileAttributes;
        *isDir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (*isDir && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) continue;

        if (WideCharToMultiByte(CP_UTF8, 0, n, -1, name, cap, NULL, NULL) <= 0) continue;
        *size = ((long long)it->data.nFileSizeHigh << 32) | (long long)it->data.nFileSizeLow;
        return 1;
    }
}

static void DwClose(DwIter* it) {
    FindClose(it->handle);
}

#else /* POSIX */

#include <dirent.h>
#include <sys/stat.h>

typedef struct DwIter {
    DIR* dir;
} DwIter;

static int DwOpen(const char* path, DwIter* it) {
    it->dir = opendir(path);
    return it->dir != NULL;
}

static int DwNext(DwIter* it, const char* dirPath, char* name, int cap, int* isDir, long long* size) {
    struct dirent* entry;
    while ((entry = readdir(it->dir)) != NULL) {
        char full[DW_PATH_MAX];
        struct stat st;
        if (entry->d_name[0] == '.' && (entry->d_name[1] == 0 || (entry->d_name[1] == '.' && entry->d_name[2] == 0)))
            continue;
        if (snprintf(full, sizeof(full), "%s/%s", dirPath, entry->d_name) >= (int)sizeof(full)) continue;
        if (lstat(full, &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue; /* sem seguir links */
        *isDir = S_ISDIR(st.st_mode) ? 1 : 0;
        if (!*isDir && !S_ISREG(st.st_mode)) continue;
        if (snprintf(name, (size_t)cap, "%s", entry->d_name) >= cap) continue;
        *size = (long long)st.st_size;
        return 1;
    }
    return 0;
}

static void DwClose(DwIter* it) {
    closedir(it->dir);
}

#endif

static void DwWalk(const char* path, int depth, int maxDepth, DirWalkTotals* totals) {
    DwIter it;
    char name[DW_NAME_MAX];
    int isDir;
    long long size;

    if (depth > maxDepth || !DwOpen(path, &it)) return;
    while (DwNext(&it, path, name, (int)sizeof(name), &isDir, &size)) {
        if (isDir) {
            char child[DW_PATH_MAX];
            ++totals->dirs;
            if (snprintf(child, sizeof(child), "%s/%s", path, name) < (int)sizeof(child))
                DwWalk(child, depth + 1, maxDepth, totals);
        } else {
            ++totals->files;
            totals->bytes += size;
        }
    }
    DwClose(&it);
}

int DirWalk_Totals(const char* path, int maxDepth, DirWalkTotals* out) {
    DwIter probe;
    DirWalkTotals totals = {0, 0, 0};
    if (out != NULL) *out = totals;
    if (path == NULL || path[0] == '\0') return 0;
    if (!DwOpen(path, &probe)) return 0;
    DwClose(&probe);
    DwWalk(path, 0, maxDepth, &totals);
    if (out != NULL) *out = totals;
    return 1;
}

int DirWalk_ForEachDir(const char* path, DirWalkNameFn cb, void* user) {
    DwIter it;
    char name[DW_NAME_MAX];
    int isDir;
    long long size;

    if (path == NULL || cb == NULL || !DwOpen(path, &it)) return 0;
    while (DwNext(&it, path, name, (int)sizeof(name), &isDir, &size)) {
        if (isDir && cb(name, user) != 0) break;
    }
    DwClose(&it);
    return 1;
}
