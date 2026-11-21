/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImport.c - Orquestracao do import: thread, progresso, cancelamento e varredura.
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/modimport/ModImportPath.h"
#include "data/ModLayout.h"
#include "data/modimport/ModImportFiles.h"
#include "data/modimport/ModImportCharacters.h"
#include "data/modimport/ModImportFormat.h"
#include "data/modimport/ModImportVSlice.h"
#include "data/modimport/ModImportAnalyze.h"

#include "Log.h"
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

/* ------------------------------------------------------------------------- */
/* API publica                                                               */
/* ------------------------------------------------------------------------- */

#define MI_TICK(name) do { const double now_ = GetTime(); \
    char tickBuf_[96]; \
    (void)snprintf(tickBuf_, sizeof(tickBuf_), "[import] %-14s %7.0f ms", name, (now_ - tickT) * 1000.0); \
    Log_Info(tickBuf_); tickT = now_; } while (0)

static char MI_ImportSingle(const char *srcMod, const char *destModPath,
                            const char *modName, char *msg, size_t msgSize,
                            ModImportStats *stats)
{
    ModImportStats localStats;
    MI_ImportContext context;
    char modNameSafe[64];
    char sourceWeeks[MI_PATH_MAX];
    char destinationWeeks[MI_PATH_MAX];
    char sourceStages[MI_PATH_MAX];
    char destinationStages[MI_PATH_MAX];
    char sourceDataStages[MI_PATH_MAX];
    char destinationDataStages[MI_PATH_MAX];
    char sourceImages[MI_PATH_MAX];
    char destinationImages[MI_PATH_MAX];
    int copiedImages = 0;
    int ok = 1;

    memset(&localStats, 0, sizeof(localStats));
    if (stats != NULL) memset(stats, 0, sizeof(*stats));
    if (msg != NULL && msgSize > 0) msg[0] = '\0';

    if (srcMod == NULL || destModPath == NULL || modName == NULL ||
        !DirectoryExists(srcMod)) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "import: origem/destino invalido");
        return 0;
    }

    MI_SanitizeModName(modNameSafe, sizeof(modNameSafe), modName);
    if (!MI_EnsureDirectory(destModPath)) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "import: nao foi possivel criar %s", destModPath);
        return 0;
    }

    memset(&context, 0, sizeof(context));
    context.srcRoot = srcMod;
    context.dstRoot = destModPath;
    context.stats = &localStats;

    if (!MI_Join(sourceWeeks, sizeof(sourceWeeks), srcMod, "weeks") ||
        !MI_Join(destinationWeeks, sizeof(destinationWeeks), destModPath, "weeks") ||
        !MI_Join(sourceStages, sizeof(sourceStages), srcMod, "stages") ||
        !MI_Join(destinationStages, sizeof(destinationStages), destModPath, "stages") ||
        !MI_Join(sourceDataStages, sizeof(sourceDataStages), srcMod, "data/stages") ||
        !MI_Join(destinationDataStages, sizeof(destinationDataStages), destModPath, "data/stages") ||
        !MI_Join(sourceImages, sizeof(sourceImages), srcMod, "images") ||
        !MI_Join(destinationImages, sizeof(destinationImages), destModPath, "images")) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "import: caminho excede o limite permitido");
        return 0;
    }

    double tickT = GetTime();
    /* Formato do mod (so rotulo + analise; a copia e a mesma para todos). */
    snprintf(localStats.format, sizeof(localStats.format), "%s",
             MI_FormatName(MI_DetectFormat(srcMod)));
    if (!MI_ImportSongs(srcMod, destModPath, &context)) ok = 0;
    /* V-Slice: charts por dificuldade viram songs/<musica>/<dificuldade>.json do engine. */
    if (!MI_ImportVSliceCharts(srcMod, destModPath, &context)) ok = 0;
    MI_TICK("songs");

    if (DirectoryExists(sourceWeeks) &&
        !MI_CopyWeeksRecursive(sourceWeeks, destinationWeeks, &context, 0)) ok = 0;
    if (DirectoryExists(sourceStages) &&
        !MI_CopyStagesRecursive(sourceStages, destinationStages, &localStats, 0)) ok = 0;
    if (DirectoryExists(sourceDataStages) &&
        !MI_CopyStagesRecursive(sourceDataStages, destinationDataStages, &localStats, 0)) ok = 0;

    MI_TICK("weeks+stages");
    if (DirectoryExists(sourceImages)) {
        if (!MI_CopyTree(sourceImages, destinationImages, &localStats.bytes,
                         &copiedImages, 0)) ok = 0;
        localStats.images += copiedImages;
    }

    MI_TICK("images (queue)");
    /* Audio e scripts do mod (o ModPath ja procura nestas pastas). */
    {
        static const char *const extraDirs[] = {"sounds", "music", "scripts"};
        for (size_t i = 0; i < sizeof(extraDirs) / sizeof(extraDirs[0]); ++i) {
            char extraSource[MI_PATH_MAX];
            char extraDestination[MI_PATH_MAX];
            int copiedExtra = 0;
            if (!MI_Join(extraSource, sizeof(extraSource), srcMod, extraDirs[i]) ||
                !MI_Join(extraDestination, sizeof(extraDestination), destModPath, extraDirs[i])) {
                ok = 0;
                continue;
            }
            if (!DirectoryExists(extraSource)) continue;
            if (!MI_CopyTree(extraSource, extraDestination, &localStats.bytes, &copiedExtra, 0))
                ok = 0;
        }
    }

    MI_TICK("audio+scripts");
    if (!MI_ImportCharacters(srcMod, destModPath, &context)) ok = 0;
    MI_TICK("characters");
    MI_CopyCollectedIcons(&context);

    MI_TICK("icons");
    MI_Drain(&localStats.bytes);
    MI_TICK("wait workers");
    if (stats != NULL) *stats = localStats;
    if (msg != NULL && msgSize > 0) {
        size_t used;
        (void)snprintf(msg, msgSize,
            "import %s: %d musicas, %d charts, %d weeks, %d stages, %d arquivos de imagem, %d personagens, %d animsets, %d icones (%lld KB)",
            modNameSafe, localStats.songs, localStats.charts, localStats.weeks,
            localStats.stages, localStats.images, localStats.characters,
            localStats.animsets, localStats.icons, localStats.bytes / 1024LL);
        used = strlen(msg);
        if (used < msgSize) MI_FormatAnalysisLine(msg + used, msgSize - used, &localStats);
    }

    /* Mods sem songs tambem podem conter stages/personagens/imagens validos. */
    if (localStats.songs == 0 && localStats.weeks == 0 && localStats.stages == 0 &&
        localStats.images == 0 && localStats.characters == 0) return 0;
    return (char)(ok != 0);
}

static char MI_RunImpl(const char *srcMod, const char *dstRoot, const char *modName,
                   char *msg, size_t msgSize, ModImportStats *stats)
{
    ModImportStats total;
    char sourceIsMods;
    char destination[MI_PATH_MAX];
    memset(&total, 0, sizeof(total));
    if (stats != NULL) memset(stats, 0, sizeof(*stats));
    if (msg != NULL && msgSize > 0) msg[0] = '\0';

    if (srcMod == NULL || dstRoot == NULL || modName == NULL ||
        !DirectoryExists(srcMod) || !MI_IsModsRoot(dstRoot)) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "import: origem/raiz invalida (destino deve ser assets/mods)");
        return 0;
    }

    sourceIsMods = (char)(MI_IsNameMods(MI_BaseName(srcMod)) || MI_IsNameMods(modName));
    if (!sourceIsMods) {
        char safeName[64];
        MI_SanitizeModName(safeName, sizeof(safeName), modName);
        if (!MI_Join(destination, sizeof(destination), dstRoot, safeName)) return 0;
        return MI_ImportSingle(srcMod, destination, safeName, msg, msgSize, stats);
    }

    /* A pasta chamada "mods" pode ser o proprio mod (com songs/weeks/etc.)
     * ou um container com varios mods. Nunca cria assets/mods/mods. */
    if (MI_HasModLayout(srcMod)) {
        return MI_ImportSingle(srcMod, dstRoot, "mods", msg, msgSize, stats);
    }

    {
        FilePathList children = MI_ListDirectories(srcMod);
        int imported = 0;
        int ok = 1;
        for (unsigned i = 0; i < children.count; ++i) {
            const char *childName = MI_BaseName(children.paths[i]);
            char safeName[64];
            ModImportStats childStats;
            char childMsg[256];
            if (!MI_IsSafeComponent(childName) || MI_IsNameMods(childName)) continue;
            MI_SanitizeModName(safeName, sizeof(safeName), childName);
            if (!MI_Join(destination, sizeof(destination), dstRoot, safeName)) {
                ok = 0;
                continue;
            }
            memset(&childStats, 0, sizeof(childStats));
            if (MI_ImportSingle(children.paths[i], destination, safeName,
                                childMsg, sizeof(childMsg), &childStats)) {
                MI_StatsAdd(&total, &childStats);
                ++imported;
            } else if (childStats.charts || childStats.songs || childStats.weeks ||
                       childStats.stages || childStats.images || childStats.characters) {
                MI_StatsAdd(&total, &childStats);
                ok = 0;
            }
        }
        UnloadDirectoryFiles(children);
        if (stats != NULL) *stats = total;
        if (msg != NULL && msgSize > 0) {
            size_t used;
            (void)snprintf(msg, msgSize,
                "import mods: %d mods, %d musicas, %d charts, %d weeks, %d stages, %d imagens, %d personagens, %d animsets, %d icones (%lld KB)",
                imported, total.songs, total.charts, total.weeks, total.stages,
                total.images, total.characters, total.animsets, total.icons,
                total.bytes / 1024LL);
            used = strlen(msg);
            if (used < msgSize) MI_FormatAnalysisLine(msg + used, msgSize - used, &total);
        }
        return (char)(imported > 0 && ok);
    }
}

static void MI_ResetCounters(void)
{
    atomic_store(&mi_cancel, 0);
    atomic_store(&mi_failed, 0);
    atomic_store(&mi_filesDone, 0);
    atomic_store(&mi_bytesDone, 0);
    atomic_store(&mi_deferredBytes, 0);
    atomic_store(&mi_dupSkipped, 0);
}

/* Versao bloqueante (sem workers). */
char ModImport_Run(const char *srcMod, const char *dstRoot, const char *modName,
                   char *msg, size_t msgSize, ModImportStats *stats)
{
    char ok;
    MI_ResetCounters();
    atomic_store(&mi_useDefer, 0);
    ok = MI_RunImpl(srcMod, dstRoot, modName, msg, msgSize, stats);
    MI_ClassifyCacheClear();
    return ok;
}

/* ---- import em segundo plano ---- */
enum { MI_RUN_IDLE = 0, MI_RUN_RUNNING = 1, MI_RUN_DONE = 2 };

static struct {
    char src[MI_PATH_MAX];
    char dst[MI_PATH_MAX];
    char name[128];
    Thread *thread;
    atomic_int state;
    atomic_int filesTotal;
    atomic_llong bytesTotal;
    char msg[384];   /* cabe a mensagem do import + analise (formato, notas, pico, custo) */
    ModImportStats stats;
    char ok;
} mi_run;

static void MI_RunThread(void *arg)
{
    DirWalkTotals totals = {0};
    (void)arg;

    if (DirWalk_Totals(mi_run.src, 64, &totals)) {
        atomic_store(&mi_run.filesTotal, totals.files);
        atomic_store(&mi_run.bytesTotal, totals.bytes);
    }
    mi_run.ok = MI_RunImpl(mi_run.src, mi_run.dst, mi_run.name,
                           mi_run.msg, sizeof(mi_run.msg), &mi_run.stats);
    MI_ClassifyCacheClear();   /* classificacao so vale dentro desta importacao */
    MI_Drain(NULL);
    atomic_store(&mi_useDefer, 0);

    if (atomic_load(&mi_cancel)) {
        mi_run.ok = 0;
        snprintf(mi_run.msg, sizeof(mi_run.msg),
                 "import cancelled (%d files copied; re-importing skips them)",
                 atomic_load(&mi_filesDone));
    } else if (atomic_load(&mi_failed) > 0) {
        char tail[64];
        snprintf(tail, sizeof(tail), " [%d files failed]", atomic_load(&mi_failed));
        size_t n = strlen(mi_run.msg);
        if (n + strlen(tail) < sizeof(mi_run.msg)) strcat(mi_run.msg, tail);
        mi_run.ok = 0;
    }
    atomic_store(&mi_run.state, MI_RUN_DONE);
}

char ModImport_Start(const char *srcMod, const char *dstRoot, const char *modName)
{
    if (srcMod == NULL || dstRoot == NULL || modName == NULL) return 0;
    if (atomic_load(&mi_run.state) != MI_RUN_IDLE) return 0;

    snprintf(mi_run.src, sizeof(mi_run.src), "%s", srcMod);
    snprintf(mi_run.dst, sizeof(mi_run.dst), "%s", dstRoot);
    snprintf(mi_run.name, sizeof(mi_run.name), "%s", modName);
    mi_run.msg[0] = '\0';
    mi_run.ok = 0;
    memset(&mi_run.stats, 0, sizeof(mi_run.stats));
    atomic_store(&mi_run.filesTotal, 0);
    atomic_store(&mi_run.bytesTotal, 0);
    MI_ResetCounters();
    atomic_store(&mi_useDefer, 1);
    atomic_store(&mi_run.state, MI_RUN_RUNNING);

    mi_run.thread = Thread_Start(MI_RunThread, NULL);
    if (mi_run.thread == NULL) {
        atomic_store(&mi_useDefer, 0);
        atomic_store(&mi_run.state, MI_RUN_IDLE);
        return 0;
    }
    return 1;
}

/* Chamar antes de Jobs_Shutdown: cancela e espera a thread do import. */
void ModImport_Shutdown(void)
{
    if (atomic_load(&mi_run.state) == MI_RUN_IDLE) return;
    atomic_store(&mi_cancel, 1);
    if (mi_run.thread != NULL) {
        Thread_Join(mi_run.thread);
        mi_run.thread = NULL;
    }
    atomic_store(&mi_useDefer, 0);
    atomic_store(&mi_run.state, MI_RUN_IDLE);
}

char ModImport_Busy(void)
{
    return atomic_load(&mi_run.state) != MI_RUN_IDLE;
}

void ModImport_Cancel(void)
{
    if (atomic_load(&mi_run.state) == MI_RUN_RUNNING) {
        atomic_store(&mi_cancel, 1);
    }
}

char ModImport_Poll(ModImportProgress *prog, char *msg, size_t msgSize,
                    ModImportStats *stats, char *ok)
{
    Jobs_Poll();
    if (prog != NULL) {
        prog->filesDone = atomic_load(&mi_filesDone);
        prog->filesTotal = atomic_load(&mi_run.filesTotal);
        prog->bytesDone = atomic_load(&mi_bytesDone);
        prog->bytesTotal = atomic_load(&mi_run.bytesTotal);
        prog->failed = atomic_load(&mi_failed);
        prog->duplicates = atomic_load(&mi_dupSkipped);
        prog->cancelling = (char)atomic_load(&mi_cancel);
    }
    if (atomic_load(&mi_run.state) != MI_RUN_DONE) return 0;

    Thread_Join(mi_run.thread);
    mi_run.thread = NULL;
    if (msg != NULL && msgSize > 0) snprintf(msg, msgSize, "%s", mi_run.msg);
    if (stats != NULL) *stats = mi_run.stats;
    if (ok != NULL) *ok = mi_run.ok;
    atomic_store(&mi_run.state, MI_RUN_IDLE);
    return 1;
}

/* Conta so as subpastas diretas (usado para contar musicas em <mod>/songs). */
static int MI_CountDirCb(const char *name, void *user)
{
    (void)name;
    ++*(int *)user;
    return 0;
}

static int MI_CountSubdirs(const char *modPath)
{
    char songsPath[MI_PATH_MAX];
    int count = 0;
    if (!MI_Join(songsPath, sizeof(songsPath), modPath, "songs")) return 0;
    (void)DirWalk_ForEachDir(songsPath, MI_CountDirCb, &count);
    return count;
}

typedef struct MI_ScanCtx {
    const char *root;
    ModImportUsage *usage;
    int mixedLayout; /* 1 = a raiz ja e um mod; so pastas fora do layout contam como mods extras */
} MI_ScanCtx;

static int MI_ScanChildCb(const char *name, void *user)
{
    MI_ScanCtx *ctx = (MI_ScanCtx *)user;
    char child[MI_PATH_MAX];

    if (!MI_IsSafeComponent(name)) return 0;
    if (ctx->mixedLayout && MI_IsLayoutDirName(name)) return 0;
    if (!MI_Join(child, sizeof(child), ctx->root, name)) return 0;

    ++ctx->usage->mods;
    ctx->usage->songs += MI_CountSubdirs(child);
    if (!ctx->mixedLayout) { /* no layout misturado os bytes ja estao no total da raiz */
        const long long bytes = MI_CountTreeBytes(child);
        if (bytes > 0 && ctx->usage->bytes <= LLONG_MAX - bytes) ctx->usage->bytes += bytes;
    }
    return 0;
}

/* Sem raylib alem de DirectoryExists (so stat): seguro para rodar numa thread do Jobs. */
char ModImport_ScanMods(const char *dstRoot, ModImportUsage *out)
{
    ModImportUsage usage;
    MI_ScanCtx ctx;
    memset(&usage, 0, sizeof(usage));
    if (out != NULL) *out = usage;
    if (!MI_IsModsRoot(dstRoot) || !DirectoryExists(dstRoot)) return 0;

    ctx.root = dstRoot;
    ctx.usage = &usage;

    if (MI_HasModLayout(dstRoot)) {
        usage.mods = 1;
        usage.bytes = MI_CountTreeBytes(dstRoot);
        usage.songs = MI_CountSubdirs(dstRoot);
        ctx.mixedLayout = 1;
    } else {
        ctx.mixedLayout = 0;
    }
    (void)DirWalk_ForEachDir(dstRoot, MI_ScanChildCb, &ctx);

    if (out != NULL) *out = usage;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Varredura em thread de fundo                                              */
/* ------------------------------------------------------------------------- */

enum { MI_SCAN_IDLE = 0, MI_SCAN_RUNNING = 1, MI_SCAN_READY = 2 };

static struct {
    char root[MI_PATH_MAX];
    ModImportUsage usage;   /* escrito pelo worker; lido so depois do JobDone */
    atomic_int state;
    atomic_int dirty;       /* pediram outra varredura enquanto esta rodava */
} mi_scan;

static void MI_ScanJob(void *data)
{
    ModImportUsage usage;
    (void)data;
    memset(&usage, 0, sizeof(usage));
    (void)ModImport_ScanMods(mi_scan.root, &usage);
    mi_scan.usage = usage;
}

static void MI_ScanDone(void *data)
{
    (void)data;
    atomic_store(&mi_scan.state, MI_SCAN_READY);
}

char ModImport_ScanStart(const char *dstRoot)
{
    if (!MI_IsModsRoot(dstRoot)) return 0;
    if (atomic_load(&mi_scan.state) != MI_SCAN_IDLE) {
        atomic_store(&mi_scan.dirty, 1); /* refaz quando esta terminar */
        return 1;
    }
    MI_CopyString(mi_scan.root, sizeof(mi_scan.root), dstRoot);
    atomic_store(&mi_scan.state, MI_SCAN_RUNNING);
    if (!Jobs_Submit(MI_ScanJob, NULL, MI_ScanDone)) { /* sem threads: faz aqui mesmo */
        MI_ScanJob(NULL);
        atomic_store(&mi_scan.state, MI_SCAN_READY);
    }
    return 1;
}

char ModImport_ScanPoll(ModImportUsage *out)
{
    Jobs_Poll();
    if (atomic_load(&mi_scan.state) != MI_SCAN_READY) return 0;

    if (atomic_exchange(&mi_scan.dirty, 0)) { /* resultado velho: varre de novo */
        atomic_store(&mi_scan.state, MI_SCAN_IDLE);
        (void)ModImport_ScanStart(mi_scan.root);
        return 0;
    }
    if (out != NULL) *out = mi_scan.usage;
    atomic_store(&mi_scan.state, MI_SCAN_IDLE);
    return 1;
}

char ModImport_ScanBusy(void)
{
    return (char)(atomic_load(&mi_scan.state) != MI_SCAN_IDLE);
}

char ModImport_DeleteAll(const char *dstRoot, char *msg, size_t msgSize,
                         int *removedMods, long long *freedBytes)
{
    FilePathList mods;
    FilePathList rootEntries;
    int modCount = 0;
    int layoutFound = 0;
    int rootIsMod = 0;
    int removedFiles = 0;
    long long bytes = 0;
    int ok = 1;

    if (removedMods != NULL) *removedMods = 0;
    if (freedBytes != NULL) *freedBytes = 0;
    if (msg != NULL && msgSize > 0) msg[0] = '\0';

    if (!MI_IsModsRoot(dstRoot)) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "delete: raiz recusada (use somente assets/mods)");
        return 0;
    }
    if (!DirectoryExists(dstRoot)) {
        if (msg != NULL && msgSize > 0)
            (void)snprintf(msg, msgSize, "delete: pasta de mods ainda nao existe");
        return 1;
    }

    bytes = MI_CountTreeBytes(dstRoot);
    rootIsMod = MI_HasModLayout(dstRoot);
    mods = MI_ListDirectories(dstRoot);
    for (unsigned i = 0; i < mods.count; ++i) {
        if (rootIsMod && MI_IsLayoutDirName(MI_BaseName(mods.paths[i]))) layoutFound = 1;
        else ++modCount;
        if (!MI_RemoveTree(mods.paths[i], 0, &removedFiles)) ok = 0;
        if (MI_RMDIR(mods.paths[i]) != 0 && DirectoryExists(mods.paths[i])) ok = 0;
    }
    UnloadDirectoryFiles(mods);
    if (layoutFound) ++modCount; /* as pastas do layout direto contam como 1 mod */

    /* Remove arquivos soltos na raiz, mas preserva a pasta assets/mods. */
    rootEntries = MI_ListEntries(dstRoot);
    for (unsigned i = 0; i < rootEntries.count; ++i) {
        if (MI_IsDirectoryPath(rootEntries.paths[i])) continue;
        if (remove(rootEntries.paths[i]) == 0) ++removedFiles;
        else ok = 0;
    }
    UnloadDirectoryFiles(rootEntries);

    if (removedMods != NULL) *removedMods = modCount;
    if (freedBytes != NULL) *freedBytes = bytes;
    if (msg != NULL && msgSize > 0) {
        (void)snprintf(msg, msgSize,
            "delete: %d mods, %d arquivos, aproximadamente %lld KB liberados em %s",
            modCount, removedFiles, bytes / 1024LL, dstRoot);
    }
    return (char)(ok != 0);
}
