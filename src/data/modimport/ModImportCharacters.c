/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportCharacters.c - Importacao de songs, weeks, stages, personagens e icones.
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/modimport/ModImportCharacters.h"
#include "data/modimport/ModImportAnalyze.h"
#include "data/modimport/ModImportVSlice.h"
#include "data/modimport/ModImportPath.h"
#include "data/modimport/ModImportFiles.h"
#include "data/modimport/ModImportParse.h"

#include "threads/cpu/Jobs.h"
#include "threads/Thread.h"

#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Icones e importacao de recursos                                            */
/* ------------------------------------------------------------------------- */

static int MI_CopyIconAlias(const char *srcMod, const char *dstMod,
                            const char *iconKey, long long *bytes)
{
    char srcIcons[MI_PATH_MAX];
    char dstIcons[MI_PATH_MAX];
    char source[MI_PATH_MAX];
    char destination[MI_PATH_MAX];
    char candidate[96];
    const char *names[2];

    if (!MI_IsSafeComponent(iconKey)) return 0;
    if (!MI_Join(srcIcons, sizeof(srcIcons), srcMod, "images/icons") ||
        !MI_Join(dstIcons, sizeof(dstIcons), dstMod, "images/icons")) return 0;
    if (!MI_EnsureDirectory(dstIcons)) return 0;
    if (snprintf(destination, sizeof(destination), "%s/%s.png", dstIcons, iconKey) >=
        (int)sizeof(destination)) return 0;

    names[0] = iconKey;
    names[1] = NULL;
    if (strncmp(iconKey, "icon-", 5) != 0) {
        (void)snprintf(candidate, sizeof(candidate), "icon-%s", iconKey);
        names[1] = candidate;
    }

    for (int i = 0; i < 2; ++i) {
        if (names[i] == NULL) continue;
        if (snprintf(source, sizeof(source), "%s/%s.png", srcIcons, names[i]) >=
            (int)sizeof(source)) continue;
        if (FileExists(source) && MI_CopyFile(source, destination, bytes)) return 1;
    }
    return 0;
}

void MI_CopyCollectedIcons(MI_ImportContext *context)
{
    for (int i = 0; i < context->iconNames.count; ++i) {
        if (MI_CopyIconAlias(context->srcRoot, context->dstRoot,
                             context->iconNames.names[i], &context->stats->bytes)) {
            ++context->stats->icons;
        }
        /* A chave de week como "icon-charlie" tambem pode ser pedida sem prefixo
         * pelo HUD de personagem. Preservamos as duas formas como alias. */
        if (strncmp(context->iconNames.names[i], "icon-", 5) == 0 &&
            context->iconNames.names[i][5] != '\0') {
            if (MI_CopyIconAlias(context->srcRoot, context->dstRoot,
                                 context->iconNames.names[i] + 5,
                                 &context->stats->bytes)) {
                ++context->stats->icons;
            }
        }
    }
}

int MI_CopyWeeksRecursive(const char *sourceDir, const char *destinationDir,
                                 MI_ImportContext *context, int depth)
{
    FilePathList entries;
    FilePathList dirs;
    int ok = 1;
    if (depth > MI_MAX_DEPTH || !DirectoryExists(sourceDir)) return depth <= MI_MAX_DEPTH;
    if (!MI_EnsureDirectory(destinationDir)) return 0;

    entries = MI_ListEntries(sourceDir);
    for (unsigned i = 0; i < entries.count; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        char destination[MI_PATH_MAX];
        if (MI_IsDirectoryPath(entries.paths[i]) || !MI_EndsWithI(base, ".json")) continue;
        if (!MI_IsSafeComponent(base) || !MI_Join(destination, sizeof(destination), destinationDir, base)) {
            ok = 0;
            continue;
        }
        MI_ParseWeekIcons(entries.paths[i], &context->iconNames);
        if (MI_CopyFile(entries.paths[i], destination, &context->stats->bytes))
            ++context->stats->weeks;
        else
            ok = 0;
    }
    UnloadDirectoryFiles(entries);

    dirs = MI_ListDirectories(sourceDir);
    for (unsigned i = 0; i < dirs.count; ++i) {
        if (!MI_CopyWeeksRecursive(dirs.paths[i], destinationDir, context, depth + 1)) ok = 0;
    }
    UnloadDirectoryFiles(dirs);
    return ok;
}

int MI_CopyStagesRecursive(const char *sourceDir, const char *destinationDir,
                                  ModImportStats *stats, int depth)
{
    FilePathList entries;
    FilePathList dirs;
    int ok = 1;
    if (depth > MI_MAX_DEPTH || !DirectoryExists(sourceDir)) return depth <= MI_MAX_DEPTH;
    if (!MI_EnsureDirectory(destinationDir)) return 0;

    entries = MI_ListEntries(sourceDir);
    for (unsigned i = 0; i < entries.count; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        char destination[MI_PATH_MAX];
        if (MI_IsDirectoryPath(entries.paths[i])) continue;
        if (!MI_EndsWithI(base, ".lua") && !MI_EndsWithI(base, ".json")) continue;
        if (!MI_IsSafeComponent(base) || !MI_Join(destination, sizeof(destination), destinationDir, base)) {
            ok = 0;
            continue;
        }
        if (MI_CopyFile(entries.paths[i], destination, &stats->bytes)) ++stats->stages;
        else ok = 0;
    }
    UnloadDirectoryFiles(entries);

    dirs = MI_ListDirectories(sourceDir);
    for (unsigned i = 0; i < dirs.count; ++i) {
        if (!MI_CopyStagesRecursive(dirs.paths[i], destinationDir, stats, depth + 1)) ok = 0;
    }
    UnloadDirectoryFiles(dirs);
    return ok;
}

#define MI_CHART_SCAN_MAX (32L * 1024L * 1024L)

/* Um unico parse do chart: icones (player1/player2) e custo para as estatisticas do import.
 * Le a ORIGEM, nunca o destino: a copia pode ser adiada para um worker e o destino ainda
 * nao existir aqui. O conteudo e o mesmo (a copia e byte a byte). */
static void MI_ScanChartOnce(const char *chartPath, MI_ImportContext *context)
{
    long size = 0;
    char *buffer = MI_ReadFile(chartPath, &size, MI_CHART_SCAN_MAX);
    yyjson_doc *doc;
    MI_ChartCost cost;

    if (buffer == NULL) return;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return;

    MI_CollectChartActors(yyjson_doc_get_root(doc), &context->iconNames);
    if (MI_AnalyzeChartRoot(yyjson_doc_get_root(doc), &cost))
        MI_AddChartCost(context->stats, &cost);
    yyjson_doc_free(doc);
}

static int MI_CopyChartsFromDirectory(const char *sourceDir, const char *destinationSong,
                                     MI_ImportContext *context, int *chartCount)
{
    FilePathList entries;
    int ok = 1;
    if (!DirectoryExists(sourceDir)) return 1;
    entries = MI_ListEntries(sourceDir);

    for (unsigned i = 0; i < entries.count; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        char destination[MI_PATH_MAX];
        if (MI_IsDirectoryPath(entries.paths[i])) continue;
        if (MI_EndsWithI(base, ".lua")) {
            ++context->stats->skippedLua;
            continue;
        }
        if (!MI_EndsWithI(base, ".json")) continue;
        if (MI_CaseEqual(base, "events.json")) {
            ++context->stats->skippedEvents;
            continue;
        }
        /* V-Slice (chart/metadata) e convertido depois por MI_ImportVSliceCharts. */
        if (MI_VSliceFileKind(entries.paths[i]) != 0) continue;
        if (!MI_IsSafeComponent(base) || !MI_Join(destination, sizeof(destination), destinationSong, base)) {
            ok = 0;
            continue;
        }
        if (FileExists(destination)) {
            /* Reimport atualiza arquivos existentes sem duplicar a estatistica. */
            (void)MI_CopyFile(entries.paths[i], destination, &context->stats->bytes);
            MI_ScanChartOnce(entries.paths[i], context);
            continue;
        }
        if (MI_CopyFile(entries.paths[i], destination, &context->stats->bytes)) {
            ++context->stats->charts;
            if (chartCount != NULL) ++*chartCount;
            MI_ScanChartOnce(entries.paths[i], context);
        } else {
            ok = 0;
        }
    }
    UnloadDirectoryFiles(entries);
    return ok;
}

static int MI_StartsWithI(const char *text, const char *prefix)
{
    if (text == NULL || prefix == NULL) return 0;
    while (*prefix) {
        unsigned char a = (unsigned char)*text++;
        unsigned char b = (unsigned char)*prefix++;
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

static void MI_CopyVoiceFiles(const char *sourceSong, const char *destinationSong,
                              MI_ImportContext *context)
{
    FilePathList entries = MI_ListEntries(sourceSong);
    for (unsigned i = 0; i < entries.count; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        char destination[MI_PATH_MAX];
        if (MI_IsDirectoryPath(entries.paths[i]) || !MI_StartsWithI(base, "Voices")) continue;
        if (!MI_EndsWithI(base, ".ogg") && !MI_EndsWithI(base, ".mp3") &&
            !MI_EndsWithI(base, ".wav")) continue;
        if (!MI_IsSafeComponent(base) ||
            !MI_Join(destination, sizeof(destination), destinationSong, base)) continue;
        (void)MI_CopyFile(entries.paths[i], destination, &context->stats->bytes);
    }
    UnloadDirectoryFiles(entries);
}

static int MI_CopySongFolder(const char *sourceSong, const char *destinationSong,
                             const char *sourceData, MI_ImportContext *context)
{
    static const char *const instrumentals[] = {"Inst.ogg", "Inst.mp3", "Inst.wav"};
    char sourceFile[MI_PATH_MAX];
    char destinationFile[MI_PATH_MAX];
    char chartDir[MI_PATH_MAX];
    int hasInst = 0;
    int charts = 0;

    if (!MI_EnsureDirectory(destinationSong)) return 0;

    /* Audio e opcional para importar charts: mods podem conter apenas dados. */
    if (sourceSong != NULL && DirectoryExists(sourceSong)) {
        for (size_t i = 0; i < sizeof(instrumentals) / sizeof(instrumentals[0]); ++i) {
            if (!MI_Join(sourceFile, sizeof(sourceFile), sourceSong, instrumentals[i])) continue;
            if (!FileExists(sourceFile)) continue;
            if (!MI_Join(destinationFile, sizeof(destinationFile), destinationSong, instrumentals[i])) continue;
            if (MI_CopyFile(sourceFile, destinationFile, &context->stats->bytes)) {
                hasInst = 1;
                break;
            }
        }
        MI_CopyVoiceFiles(sourceSong, destinationSong, context);
    }

    /* O formato Psych guarda os charts em data/<song>; o destino do engine
     * padroniza tudo em songs/<song>/<chart>.json. */
    if (sourceData != NULL && sourceData[0] != '\0' && DirectoryExists(sourceData))
        (void)MI_CopyChartsFromDirectory(sourceData, destinationSong, context, &charts);

    if (sourceSong != NULL && DirectoryExists(sourceSong)) {
        (void)MI_CopyChartsFromDirectory(sourceSong, destinationSong, context, &charts);
        if (MI_Join(chartDir, sizeof(chartDir), sourceSong, "charts") && DirectoryExists(chartDir))
            (void)MI_CopyChartsFromDirectory(chartDir, destinationSong, context, &charts);
    }

    if (charts > 0) ++context->stats->songs;
    return (hasInst || charts > 0);
}

static void MI_ImportLooseSongCharts(const char *sourceSongs, const char *destinationSongs,
                                     MI_ImportContext *context)
{
    FilePathList entries;
    if (!DirectoryExists(sourceSongs) || !MI_EnsureDirectory(destinationSongs)) return;
    entries = MI_ListEntries(sourceSongs);
    for (unsigned i = 0; i < entries.count; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        char songName[MI_NAME_MAX];
        char destinationSong[MI_PATH_MAX];
        if (MI_IsDirectoryPath(entries.paths[i]) || !MI_EndsWithI(base, ".json")) continue;
        if (MI_CaseEqual(base, "events.json")) {
            ++context->stats->skippedEvents;
            continue;
        }
        if (MI_VSliceFileKind(entries.paths[i]) != 0) continue;
        MI_GetStem(base, songName, sizeof(songName));
        if (!MI_IsSafeComponent(songName) || songName[0] == '\0' ||
            !MI_Join(destinationSong, sizeof(destinationSong), destinationSongs, songName)) continue;
        (void)MI_EnsureDirectory(destinationSong);
        {
            char destination[MI_PATH_MAX];
            if (MI_Join(destination, sizeof(destination), destinationSong, base)) {
                if (FileExists(destination)) {
                    (void)MI_CopyFile(entries.paths[i], destination, &context->stats->bytes);
                } else if (MI_CopyFile(entries.paths[i], destination, &context->stats->bytes)) {
                    ++context->stats->charts;
                    ++context->stats->songs;
                }
                MI_ScanChartOnce(entries.paths[i], context);
            }
        }
    }
    UnloadDirectoryFiles(entries);
}

/* A pasta tem algum chart (.json que nao seja events.json)? */
static int MI_DirHasChartJson(const char *dir)
{
    FilePathList entries = MI_ListEntries(dir);
    int found = 0;
    for (unsigned i = 0; i < entries.count && !found; ++i) {
        const char *base = MI_BaseName(entries.paths[i]);
        if (MI_IsDirectoryPath(entries.paths[i])) continue;
        if (MI_EndsWithI(base, ".json") && !MI_CaseEqual(base, "events.json")) found = 1;
    }
    UnloadDirectoryFiles(entries);
    return found;
}

int MI_ImportSongs(const char *srcMod, const char *dstMod,
                          MI_ImportContext *context)
{
    char sourceSongs[MI_PATH_MAX];
    char sourceData[MI_PATH_MAX];
    char destinationSongs[MI_PATH_MAX];
    FilePathList songDirs;
    FilePathList dataDirs;
    int haveSongs;
    int haveData;

    if (!MI_Join(sourceSongs, sizeof(sourceSongs), srcMod, "songs") ||
        !MI_Join(sourceData, sizeof(sourceData), srcMod, "data") ||
        !MI_Join(destinationSongs, sizeof(destinationSongs), dstMod, "songs")) return 0;

    haveSongs = DirectoryExists(sourceSongs);
    haveData = DirectoryExists(sourceData);
    if (!haveSongs && !haveData) return 1;
    if (!MI_EnsureDirectory(destinationSongs)) return 0;

    if (haveSongs) {
        songDirs = MI_ListDirectories(sourceSongs);
        for (unsigned i = 0; i < songDirs.count; ++i) {
            char destinationSong[MI_PATH_MAX];
            char sourceChart[MI_PATH_MAX];
            const char *songName = MI_BaseName(songDirs.paths[i]);
            if (!MI_IsSafeComponent(songName) ||
                !MI_Join(destinationSong, sizeof(destinationSong), destinationSongs, songName)) continue;

            sourceChart[0] = '\0';
            if (haveData && MI_Join(sourceChart, sizeof(sourceChart), sourceData, songName) &&
                !DirectoryExists(sourceChart)) sourceChart[0] = '\0';

            (void)MI_CopySongFolder(songDirs.paths[i], destinationSong,
                                    sourceChart[0] ? sourceChart : NULL, context);
        }
        UnloadDirectoryFiles(songDirs);
        MI_ImportLooseSongCharts(sourceSongs, destinationSongs, context);
    }

    /* Charts podem existir mesmo quando o mod nao traz a pasta songs/<nome>.
     * Importa os que ainda nao foram associados acima, mantendo-os em songs/. */
    if (haveData) {
        dataDirs = MI_ListDirectories(sourceData);
        for (unsigned i = 0; i < dataDirs.count; ++i) {
            char matchingSong[MI_PATH_MAX];
            char destinationSong[MI_PATH_MAX];
            const char *songName = MI_BaseName(dataDirs.paths[i]);
            if (MI_CaseEqual(songName, "stages") || !MI_IsSafeComponent(songName) ||
                !MI_Join(destinationSong, sizeof(destinationSong), destinationSongs, songName)) continue;
            if (haveSongs && MI_Join(matchingSong, sizeof(matchingSong), sourceSongs, songName) &&
                DirectoryExists(matchingSong)) continue;
            /* data/ tambem guarda outras coisas (credits etc.): so e musica com chart. */
            if (!MI_DirHasChartJson(dataDirs.paths[i])) continue;
            (void)MI_CopySongFolder(NULL, destinationSong, dataDirs.paths[i], context);
        }
        UnloadDirectoryFiles(dataDirs);
    }
    return 1;
}

/* Um personagem: copia o .json, le o JSON e gera o .animset. Roda em worker
 * ou na thread do import; so usa dados proprios (resultado em MiCharTask). */
typedef struct MiCharTask {
    char jsonPath[MI_PATH_MAX];
    char destJson[MI_PATH_MAX];
    char stem[MI_NAME_MAX];
    const char *sourceImageRoot;
    const char *destImageChars;
    long long bytes;
    long long animBytes;
    char healthicon[64];
    char copyOk;
    char parsed;
    char animset;
} MiCharTask;

static atomic_int mi_charInflight;

static void MI_CharTaskFn(void *data)
{
    MiCharTask *t = (MiCharTask *)data;
    MI_CharInfo *info;

    if (!atomic_load(&mi_cancel)) {
        /* sem MI_CopyFile: dentro de worker nao pode esperar vaga no Jobs */
        t->copyOk = MI_CopyFileNow(t->jsonPath, t->destJson, &t->bytes);
        atomic_fetch_add(&mi_filesDone, 1);
    }
    info = t->copyOk ? (MI_CharInfo *)malloc(sizeof(*info)) : NULL;
    if (info != NULL && MI_ParseCharacterJson(t->jsonPath, t->stem, info)) {
        char atlasXml[MI_PATH_MAX];
        char animsetPath[MI_PATH_MAX];
        const char *imagePath = info->imagePath[0] ? info->imagePath : "characters";
        int xmlFound = 0;

        t->parsed = 1;
        snprintf(t->healthicon, sizeof(t->healthicon), "%s", info->healthicon);
        if (snprintf(atlasXml, sizeof(atlasXml), "%s/%s.xml", t->sourceImageRoot, imagePath) <
            (int)sizeof(atlasXml) && FileExists(atlasXml)) {
            xmlFound = 1;
        } else {
            const char *imageBase = MI_BaseName(imagePath);
            if (snprintf(atlasXml, sizeof(atlasXml), "%s/characters/%s.xml",
                         t->sourceImageRoot, imageBase) < (int)sizeof(atlasXml) &&
                FileExists(atlasXml)) xmlFound = 1;
        }
        if (xmlFound && !atomic_load(&mi_cancel) &&
            snprintf(animsetPath, sizeof(animsetPath), "%s/%s.animset",
                     t->destImageChars, t->stem) < (int)sizeof(animsetPath) &&
            MI_WriteAnimset(info, atlasXml, animsetPath) > 0) {
            t->animset = 1;
            t->animBytes = MI_FileSize(animsetPath);
        }
    }
    free(info);
    atomic_fetch_sub(&mi_charInflight, 1);
}

int MI_ImportCharacters(const char *srcMod, const char *dstMod,
                               MI_ImportContext *context)
{
    char sourceChars[MI_PATH_MAX];
    char destinationChars[MI_PATH_MAX];
    char sourceImageRoot[MI_PATH_MAX];
    char destinationImageChars[MI_PATH_MAX];
    FilePathList entries;
    int ok = 1;

    if (!MI_Join(sourceChars, sizeof(sourceChars), srcMod, "characters") ||
        !MI_Join(destinationChars, sizeof(destinationChars), dstMod, "characters") ||
        !MI_Join(sourceImageRoot, sizeof(sourceImageRoot), srcMod, "images") ||
        !MI_Join(destinationImageChars, sizeof(destinationImageChars), dstMod, "images/characters"))
        return 0;
    if (!DirectoryExists(sourceChars)) return 1;
    if (!MI_EnsureDirectory(destinationChars)) return 0;

    entries = MI_ListEntries(sourceChars);
    {
        MiCharTask *tasks = (MiCharTask *)calloc(entries.count > 0 ? entries.count : 1, sizeof(*tasks));
        const int cap = Jobs_ThreadCount() > 1 ? Jobs_ThreadCount() : 1;
        unsigned used = 0;

        if (tasks == NULL) {
            UnloadDirectoryFiles(entries);
            return 0;
        }
        for (unsigned i = 0; i < entries.count; ++i) {
            MiCharTask *t = &tasks[used];
            const char *base = MI_BaseName(entries.paths[i]);

            if (MI_IsDirectoryPath(entries.paths[i]) || !MI_EndsWithI(base, ".json") ||
                !MI_IsSafeComponent(base)) continue;
            if (!MI_Join(t->destJson, sizeof(t->destJson), destinationChars, base)) {
                ok = 0;
                continue;
            }
            snprintf(t->jsonPath, sizeof(t->jsonPath), "%s", entries.paths[i]);
            MI_GetStem(base, t->stem, sizeof(t->stem));
            t->sourceImageRoot = sourceImageRoot;
            t->destImageChars = destinationImageChars;
            ++used;

            /* um personagem por worker (limitado); sem Jobs, roda aqui mesmo */
            if (atomic_load(&mi_useDefer) && !atomic_load(&mi_cancel)) {
                while (!atomic_load(&mi_cancel) && atomic_load(&mi_charInflight) >= cap)
                    Thread_SleepMs(1);
                atomic_fetch_add(&mi_charInflight, 1);
                if (Jobs_Submit(MI_CharTaskFn, t, NULL)) continue;
                atomic_fetch_sub(&mi_charInflight, 1);
            }
            atomic_fetch_add(&mi_charInflight, 1);
            MI_CharTaskFn(t);
        }
        while (atomic_load(&mi_charInflight) > 0) Thread_SleepMs(1);

        /* junta os resultados na ordem original */
        for (unsigned i = 0; i < used; ++i) {
            const MiCharTask *t = &tasks[i];

            context->stats->bytes += t->bytes;
            if (!t->copyOk) {
                ok = 0;
                continue;
            }
            ++context->stats->characters;
            MI_AddName(&context->iconNames, t->stem);
            if (t->parsed) {
                if (t->animset) {
                    ++context->stats->animsets;
                    context->stats->bytes += t->animBytes;
                }
                MI_AddName(&context->iconNames, t->healthicon);
            }
            MI_AddName(&context->iconNames, t->stem);
        }
        free(tasks);
    }
    UnloadDirectoryFiles(entries);
    return ok;
}
