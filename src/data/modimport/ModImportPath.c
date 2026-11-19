/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportPath.c - Caminhos e nomes (sanitizacao, juncao, criacao de pastas).
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/modimport/ModImportPath.h"

#include "core/DirWalk.h"
#include "threads/cpu/Jobs.h"
#include "threads/Thread.h"
#include "yyjson.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Caminhos e nomes                                                          */
/* ------------------------------------------------------------------------- */

const char *MI_BaseName(const char *path)
{
    const char *slash1;
    const char *slash2;
    const char *base;

    if (path == NULL) return "";
    slash1 = strrchr(path, '/');
    slash2 = strrchr(path, '\\');
    base = path;
    if (slash1 != NULL && (base == path || slash1 > base)) base = slash1 + 1;
    if (slash2 != NULL && (base == path || slash2 > base)) base = slash2 + 1;
    return base;
}

int MI_CaseEqual(const char *a, const char *b)
{
    if (a == NULL || b == NULL) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

int MI_EndsWithI(const char *text, const char *suffix)
{
    size_t textLen;
    size_t suffixLen;
    if (text == NULL || suffix == NULL) return 0;
    textLen = strlen(text);
    suffixLen = strlen(suffix);
    return suffixLen <= textLen &&
           MI_CaseEqual(text + textLen - suffixLen, suffix);
}

int MI_IsSafeComponent(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;
    if (name == NULL || name[0] == '\0') return 0;
    if (strstr(name, "..") != NULL) return 0;
    for (; *p; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p < 32) return 0;
    }
    return 1;
}

void MI_SanitizeModName(char *dst, size_t dstSize, const char *src)
{
    size_t out = 0;
    if (dst == NULL || dstSize == 0) return;
    if (src == NULL) src = "mod";

    for (size_t i = 0; src[i] != '\0' && out + 1 < dstSize; ++i) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            dst[out++] = (char)c;
        } else if (c == ' ' || c == '.') {
            dst[out++] = '_';
        }
    }

    while (out > 0 && dst[out - 1] == '_') --out;
    if (out == 0) {
        const char fallback[] = "mod";
        size_t n = sizeof(fallback) - 1;
        if (n >= dstSize) n = dstSize - 1;
        memcpy(dst, fallback, n);
        out = n;
    }
    dst[out] = '\0';
}

void MI_CopyString(char *dst, size_t dstSize, const char *src)
{
    size_t length = 0;
    if (dst == NULL || dstSize == 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (length + 1u < dstSize && src[length] != '\0') ++length;
    memcpy(dst, src, length);
    dst[length] = '\0';
}

int MI_Join(char *out, size_t outSize, const char *a, const char *b)
{
    int written;
    size_t len;
    if (out == NULL || outSize == 0 || a == NULL || b == NULL) return 0;
    len = strlen(a);
    if (len > 0 && (a[len - 1] == '/' || a[len - 1] == '\\'))
        written = snprintf(out, outSize, "%s%s", a, b);
    else
        written = snprintf(out, outSize, "%s/%s", a, b);
    return written >= 0 && (size_t)written < outSize;
}

int MI_EnsureDirectory(const char *path)
{
    char buffer[MI_PATH_MAX];
    size_t len;

    if (path == NULL || path[0] == '\0') return 0;
    len = strlen(path);
    if (len >= sizeof(buffer)) return 0;
    memcpy(buffer, path, len + 1);

    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] == '\\') buffer[i] = '/';
    }

    for (size_t i = 1; i <= len; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\0') {
            char saved = buffer[i];
            buffer[i] = '\0';
            if (buffer[0] != '\0' && !DirectoryExists(buffer)) {
                (void)MI_MKDIR(buffer);
                if (!DirectoryExists(buffer)) {
                    buffer[i] = saved;
                    return 0;
                }
            }
            buffer[i] = saved;
        }
    }
    return DirectoryExists(buffer);
}

int MI_EnsureParent(const char *filePath)
{
    char parent[MI_PATH_MAX];
    char *slash;
    size_t len;
    if (filePath == NULL) return 0;
    len = strlen(filePath);
    if (len >= sizeof(parent)) return 0;
    memcpy(parent, filePath, len + 1);
    slash = strrchr(parent, '/');
    {
        char *backslash = strrchr(parent, '\\');
        if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
    }
    if (slash == NULL) return 1;
    *slash = '\0';
    return MI_EnsureDirectory(parent);
}
