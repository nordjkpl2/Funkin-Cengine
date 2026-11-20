/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportFiles.c - Copia e remocao de arquivos/pastas (nativa e em workers).
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/modimport/ModImportFiles.h"
#include "data/modimport/ModImportPath.h"

#include "core/DirWalk.h"
#include "threads/cpu/Jobs.h"
#include "threads/Thread.h"

#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void MI_StatsAdd(ModImportStats *dst, const ModImportStats *src)
{
    if (dst == NULL || src == NULL) return;
    dst->songs += src->songs;
    dst->charts += src->charts;
    dst->weeks += src->weeks;
    dst->stages += src->stages;
    dst->images += src->images;
    dst->characters += src->characters;
    dst->animsets += src->animsets;
    dst->icons += src->icons;
    if (src->bytes > 0 && dst->bytes <= LLONG_MAX - src->bytes)
        dst->bytes += src->bytes;
    dst->skippedEvents += src->skippedEvents;
    dst->skippedLua += src->skippedLua;
}

void MI_GetStem(const char *path, char *out, size_t outSize)
{
    const char *base = MI_BaseName(path);
    size_t len;
    char *dot;
    if (out == NULL || outSize == 0) return;
    len = strlen(base);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, base, len);
    out[len] = '\0';
    dot = strrchr(out, '.');
    if (dot != NULL && dot != out) *dot = '\0';
}

/* ------------------------------------------------------------------------- */
/* Arquivos e diretorios                                                     */
/* ------------------------------------------------------------------------- */

long MI_FileSize(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    fclose(file);
    return size > 0 ? size : 0;
}

char *MI_ReadFile(const char *path, long *outSize, long maxSize)
{
    FILE *file;
    long size;
    char *buffer;

    if (outSize != NULL) *outSize = 0;
    file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size <= 0 || size > maxSize || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);
    buffer[size] = '\0';
    if (outSize != NULL) *outSize = size;
    return buffer;
}

#ifdef _WIN32
#define MI_STAT _stati64
#define MI_STAT_T struct _stat64
#else
#define MI_STAT stat
#define MI_STAT_T struct stat
#endif

atomic_int mi_dupSkipped;
atomic_int mi_cancel;          /* 1 = cancelar o import em andamento */
atomic_int mi_useDefer;        /* 1 = arquivos grandes vao para o Jobs */
static atomic_int mi_inflight;        /* copias grandes em andamento */
atomic_int mi_failed;          /* copias que falharam em worker */
atomic_int mi_filesDone;       /* arquivos tratados (progresso) */
atomic_llong mi_bytesDone;     /* bytes escritos (progresso) */
atomic_llong mi_deferredBytes; /* bytes dos workers ainda nao somados nas estatisticas */

#define MI_DEFER_MIN       (2LL * 1024 * 1024)


/* destino ja existe com mesmo tamanho: considera o mesmo arquivo */
/* Destino ja existe com o mesmo tamanho: considera o mesmo arquivo. */
static int MI_SameSizeAtDest(const char *source, const char *destination)
{
    MI_STAT_T a, b;
    if (MI_STAT(destination, &b) != 0) return 0;
    if (MI_STAT(source, &a) != 0) return 0;
    return a.st_size == b.st_size;
}

#ifdef _WIN32
/* Copia nativa do Windows (CopyFileExW). Prototipos declarados aqui para
 * nao incluir windows.h junto com o raylib (conflito de nomes). */
#define MI_NATIVE_MIN        (1LL * 1024 * 1024)   /* abaixo disso usa fopen */
#define MI_NATIVE_UNBUF_MIN  (64LL * 1024 * 1024)  /* COPY_FILE_NO_BUFFERING */

typedef unsigned long MI_DWORD;
typedef unsigned long (__stdcall *MI_CopyProgressFn)(long long total, long long transferred,
    long long streamSize, long long streamTransferred, MI_DWORD streamNo, MI_DWORD reason,
    void *src, void *dst, void *data);

__declspec(dllimport) int __stdcall CopyFileExW(const wchar_t *src, const wchar_t *dst,
    MI_CopyProgressFn progress, void *data, int *cancel, MI_DWORD flags);
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned cp, MI_DWORD flags,
    const char *str, int len, wchar_t *out, int outLen);
__declspec(dllimport) int __stdcall SetFileAttributesW(const wchar_t *path, MI_DWORD attrs);

typedef struct MiNativeState { long long last; } MiNativeState;

static unsigned long __stdcall MI_NativeProgress(long long total, long long transferred,
    long long streamSize, long long streamTransferred, MI_DWORD streamNo, MI_DWORD reason,
    void *src, void *dst, void *data)
{
    MiNativeState *st = (MiNativeState *)data;
    (void)total; (void)streamSize; (void)streamTransferred; (void)streamNo; (void)reason;
    (void)src; (void)dst;
    atomic_fetch_add(&mi_bytesDone, transferred - st->last);
    st->last = transferred;
    return atomic_load(&mi_cancel) ? 1u /* PROGRESS_CANCEL */ : 0u /* PROGRESS_CONTINUE */;
}

/* 1 = copiou, 0 = falhou/cancelou (progresso desfeito; chamador cai no fopen). */
static char MI_CopyFileNative(const char *source, const char *destination, long long size)
{
    wchar_t ws[MI_PATH_MAX];
    wchar_t wd[MI_PATH_MAX];
    MiNativeState st = {0};
    MI_DWORD flags = (size >= MI_NATIVE_UNBUF_MIN) ? 0x1000u /* NO_BUFFERING */ : 0u;

    if (MultiByteToWideChar(65001, 0, source, -1, ws, MI_PATH_MAX) <= 0) return 0;
    if (MultiByteToWideChar(65001, 0, destination, -1, wd, MI_PATH_MAX) <= 0) return 0;

    if (!CopyFileExW(ws, wd, MI_NativeProgress, &st, NULL, flags)) {
        atomic_fetch_sub(&mi_bytesDone, st.last);
        (void)remove(destination);
        return 0;
    }
    SetFileAttributesW(wd, 0x80u /* NORMAL: tira somente-leitura herdado */);
    return 1;
}
#endif

char MI_CopyFileNow(const char *source, const char *destination, long long *bytes)
{
    FILE *in;
    FILE *out;
    unsigned char buffer[MI_COPY_BUFFER];
    size_t count;
    long long total = 0;
    int ok = 1;

    if (source == NULL || destination == NULL || !FileExists(source)) return 0;
    if (strcmp(source, destination) == 0) return 1;
    if (MI_SameSizeAtDest(source, destination)) {
        atomic_fetch_add(&mi_dupSkipped, 1);
        return 1;
    }
    if (!MI_EnsureParent(destination)) return 0;

#ifdef _WIN32
    {
        MI_STAT_T sst;
        if (MI_STAT(source, &sst) == 0 && (long long)sst.st_size >= MI_NATIVE_MIN) {
            if (MI_CopyFileNative(source, destination, (long long)sst.st_size)) {
                if (bytes != NULL) *bytes += (long long)sst.st_size;
                return 1;
            }
            if (atomic_load(&mi_cancel)) return 0;
            /* senao: cai no caminho fopen abaixo */
        }
    }
#endif

    in = fopen(source, "rb");
    if (in == NULL) return 0;
    out = fopen(destination, "wb");
    if (out == NULL) {
        fclose(in);
        return 0;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (atomic_load(&mi_cancel)) {
            ok = 0;
            break;
        }
        if (fwrite(buffer, 1, count, out) != count) {
            ok = 0;
            break;
        }
        total += (long long)count;
    }
    if (ferror(in)) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;

    if (!ok) {
        (void)remove(destination);
        return 0;
    }
    if (bytes != NULL) *bytes += total;
    atomic_fetch_add(&mi_bytesDone, total);
    return 1;
}

typedef struct MiCopyTask {
    char src[MI_PATH_MAX];
    char dst[MI_PATH_MAX];
} MiCopyTask;

static void MI_CopyTaskFn(void *data)
{
    MiCopyTask *t = (MiCopyTask *)data;
    long long b = 0;

    if (!atomic_load(&mi_cancel) && !MI_CopyFileNow(t->src, t->dst, &b))
        atomic_fetch_add(&mi_failed, 1);
    atomic_fetch_add(&mi_deferredBytes, b);
    atomic_fetch_add(&mi_filesDone, 1);
    atomic_fetch_sub(&mi_inflight, 1);
    free(t);
}

/* 1 = a copia foi para um worker; 0 = copiar aqui mesmo. */
static int MI_Defer(const char *source, const char *destination)
{
    MiCopyTask *t;
    const int cap = Jobs_ThreadCount() * 2 > 2 ? Jobs_ThreadCount() * 2 : 2;

    /* limita quantas copias ficam na fila do Jobs */
    while (!atomic_load(&mi_cancel) && atomic_load(&mi_inflight) >= cap)
        Thread_SleepMs(2);
    if (atomic_load(&mi_cancel)) return 0;

    t = (MiCopyTask *)malloc(sizeof(*t));
    if (t == NULL) return 0;
    snprintf(t->src, sizeof(t->src), "%s", source);
    snprintf(t->dst, sizeof(t->dst), "%s", destination);
    atomic_fetch_add(&mi_inflight, 1);
    if (!Jobs_Submit(MI_CopyTaskFn, t, NULL)) {
        atomic_fetch_sub(&mi_inflight, 1);
        free(t);
        return 0;
    }
    return 1;
}

/* Espera as copias em worker e soma os bytes delas. */
void MI_Drain(long long *bytes)
{
    while (atomic_load(&mi_inflight) > 0) Thread_SleepMs(2);
    if (bytes != NULL) *bytes += atomic_exchange(&mi_deferredBytes, 0);
    else (void)atomic_exchange(&mi_deferredBytes, 0);
}

char MI_CopyFile(const char *source, const char *destination, long long *bytes)
{
    char r;
    MI_STAT_T st;

    if (atomic_load(&mi_cancel)) return 0;
    if (atomic_load(&mi_useDefer) && source != NULL && destination != NULL &&
        strcmp(source, destination) != 0 && MI_STAT(source, &st) == 0 &&
        (long long)st.st_size >= MI_DEFER_MIN) {
        if (MI_SameSizeAtDest(source, destination)) {
            atomic_fetch_add(&mi_dupSkipped, 1);
            atomic_fetch_add(&mi_filesDone, 1);
            return 1;
        }
        if (MI_EnsureParent(destination) && MI_Defer(source, destination))
            return 1;
    }
    r = MI_CopyFileNow(source, destination, bytes);
    atomic_fetch_add(&mi_filesDone, 1);
    return r;
}

int MI_IsDirectoryPath(const char *path)
{
    return path != NULL && DirectoryExists(path);
}

FilePathList MI_ListDirectories(const char *path)
{
    return LoadDirectoryFilesEx(path, "DIR", false);
}

FilePathList MI_ListEntries(const char *path)
{
    return LoadDirectoryFilesEx(path, NULL, false);
}

int MI_CopyTree(const char *source, const char *destination,
                       long long *bytes, int *copiedFiles, int depth)
{
    FilePathList dirs;
    FilePathList entries;
    int ok = 1;

    if (depth > MI_MAX_DEPTH) return 0;
    if (!MI_EnsureDirectory(destination)) return 0;

    dirs = MI_ListDirectories(source);
    for (unsigned i = 0; i < dirs.count; ++i) {
        char childDst[MI_PATH_MAX];
        const char *base = MI_BaseName(dirs.paths[i]);
        if (!MI_IsSafeComponent(base)) continue;
        if (!MI_Join(childDst, sizeof(childDst), destination, base) ||
            !MI_CopyTree(dirs.paths[i], childDst, bytes, copiedFiles, depth + 1)) {
            ok = 0;
        }
    }
    UnloadDirectoryFiles(dirs);

    entries = MI_ListEntries(source);
    for (unsigned i = 0; i < entries.count; ++i) {
        char childDst[MI_PATH_MAX];
        const char *base = MI_BaseName(entries.paths[i]);
        if (MI_IsDirectoryPath(entries.paths[i]) || !MI_IsSafeComponent(base)) continue;
        if (!MI_Join(childDst, sizeof(childDst), destination, base) ||
            !MI_CopyFile(entries.paths[i], childDst, bytes)) {
            ok = 0;
        } else if (copiedFiles != NULL) {
            ++*copiedFiles;
        }
    }
    UnloadDirectoryFiles(entries);
    return ok;
}

/* Uma passada so (DirWalk): o tamanho vem da propria listagem, sem abrir cada arquivo. */
long long MI_CountTreeBytes(const char *path)
{
    DirWalkTotals totals;
    if (!DirWalk_Totals(path, MI_MAX_DEPTH, &totals)) return 0;
    return totals.bytes;
}

int MI_RemoveTree(const char *path, int depth, int *removedFiles)
{
    FilePathList entries;
    FilePathList dirs;
    int ok = 1;

    if (depth > MI_MAX_DEPTH) return 0;

    entries = MI_ListEntries(path);
    for (unsigned i = 0; i < entries.count; ++i) {
        if (MI_IsDirectoryPath(entries.paths[i])) continue;
        if (remove(entries.paths[i]) == 0) {
            if (removedFiles != NULL) ++*removedFiles;
        } else {
            ok = 0;
        }
    }
    UnloadDirectoryFiles(entries);

    dirs = MI_ListDirectories(path);
    for (unsigned i = 0; i < dirs.count; ++i) {
        if (!MI_RemoveTree(dirs.paths[i], depth + 1, removedFiles)) ok = 0;
        if (MI_RMDIR(dirs.paths[i]) != 0 && DirectoryExists(dirs.paths[i])) ok = 0;
    }
    UnloadDirectoryFiles(dirs);
    return ok;
}

/* ------------------------------------------------------------------------- */
