/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongChartFind.c - descoberta de chart (JSON) em pastas: layouts de assets/songs, mods Psych e
 * conversoes. Os helpers de caminho tambem ficam aqui.
 */
#include "std.h"
#include "data/song/Song.h"
#include "data/song/SongInternal.h"

#include <stdio.h>
#include <string.h>

const char *Song_BaseName(const char *path)
{
    const char *base = path;
    if (path == NULL) return "";
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

static int Song_EndsWithI(const char *text, const char *suffix)
{
    size_t textLen;
    size_t suffixLen;
    if (text == NULL || suffix == NULL) return 0;
    textLen = strlen(text);
    suffixLen = strlen(suffix);
    if (suffixLen > textLen) return 0;
    text += textLen - suffixLen;
    for (size_t i = 0; i < suffixLen; ++i) {
        unsigned char a = (unsigned char)text[i];
        unsigned char b = (unsigned char)suffix[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
        if (a != b) return 0;
    }
    return 1;
}

static int Song_PathJoin(char *out, size_t outSize, const char *left, const char *right)
{
    size_t len;
    int written;
    if (out == NULL || outSize == 0 || left == NULL || right == NULL) return 0;
    len = strlen(left);
    if (len > 0 && (left[len - 1] == '/' || left[len - 1] == '\\'))
        written = snprintf(out, outSize, "%s%s", left, right);
    else
        written = snprintf(out, outSize, "%s/%s", left, right);
    return written >= 0 && (size_t)written < outSize;
}

void Song_NormalizePath(const char *src, char *out, size_t outSize)
{
    size_t i = 0;
    if (out == NULL || outSize == 0) return;
    out[0] = '\0';
    if (src == NULL) return;
    while (src[i] != '\0' && i + 1u < outSize) {
        out[i] = src[i] == '\\' ? '/' : src[i];
        ++i;
    }
    out[i] = '\0';
}

void Song_GetModRoot(const char *songDir, char *out, size_t outSize)
{
    char normalized[SONG_PATH_MAX];
    const char marker[] = "assets/mods/";
    char *markerPos;
    char *modStart;
    char *separator;
    size_t length;

    if (out == NULL || outSize == 0) return;
    out[0] = '\0';
    if (songDir == NULL || songDir[0] == '\0') return;

    Song_NormalizePath(songDir, normalized, sizeof(normalized));
    markerPos = strstr(normalized, marker);
    if (markerPos == NULL) return;

    modStart = markerPos + sizeof(marker) - 1u;
    separator = strchr(modStart, '/');
    if (separator == NULL) return;

    /* Direct-root mod: assets/mods/songs/<song> belongs to assets/mods. */
    if ((size_t)(separator - modStart) == 5u && memcmp(modStart, "songs", 5u) == 0) {
        length = (size_t)(modStart - normalized);
        if (length >= outSize) return;
        memcpy(out, normalized, length);
        out[length] = '\0';
        return;
    }

    /* Old imports used assets/mods/mods/<modName>/... . */
    if ((size_t)(separator - modStart) == 4u && memcmp(modStart, "mods", 4u) == 0) {
        modStart = separator + 1;
        separator = strchr(modStart, '/');
        if (separator == NULL) return;
    }

    length = (size_t)(separator - normalized);
    if (length >= outSize) return;
    memcpy(out, normalized, length);
    out[length] = '\0';
}

static int Song_IsEventsFile(const char *path)
{
    return Song_EndsWithI(Song_BaseName(path), "events.json");
}

static int Song_IsDifficultyFile(const char *path)
{
    const char *base = Song_BaseName(path);
    return Song_EndsWithI(base, "-easy.json") ||
           Song_EndsWithI(base, "-normal.json") ||
           Song_EndsWithI(base, "-hard.json");
}

static int Song_TryChartFile(Song *song, const char *path, char streaming)
{
    if (path == NULL || !FileExists(path) || Song_IsEventsFile(path)) return 0;
    return Song_ParseJSON(song, path, streaming);
}

static int Song_TryLoadChartDirectory(Song *song, const char *directory,
                                      const char *diff, char streaming)
{
    char candidate[SONG_PATH_MAX];
    char baseDiff[SONG_PATH_MAX];
    const char *base;
    FilePathList files;
    int loaded = 0;

    if (song == NULL || directory == NULL || directory[0] == '\0' ||
        !DirectoryExists(directory)) return 0;
    base = Song_BaseName(directory);

    /* Exact difficulty names first. */
    if (diff != NULL && diff[0] != '\0' && strcmp(diff, "normal") != 0) {
        (void)snprintf(candidate, sizeof(candidate), "%s/data-%s.json", directory, diff);
        if (Song_TryChartFile(song, candidate, streaming)) return 1;
        if (snprintf(baseDiff, sizeof(baseDiff), "%s-%s.json", base, diff) < (int)sizeof(baseDiff)) {
            (void)snprintf(candidate, sizeof(candidate), "%s/%s", directory, baseDiff);
            if (Song_TryChartFile(song, candidate, streaming)) return 1;
        }
        (void)snprintf(candidate, sizeof(candidate), "%s/%s.json", directory, diff);
        if (Song_TryChartFile(song, candidate, streaming)) return 1;
    } else {
        (void)snprintf(candidate, sizeof(candidate), "%s/normal.json", directory);
        if (Song_TryChartFile(song, candidate, streaming)) return 1;
        if (snprintf(baseDiff, sizeof(baseDiff), "%s-normal.json", base) < (int)sizeof(baseDiff)) {
            (void)snprintf(candidate, sizeof(candidate), "%s/%s", directory, baseDiff);
            if (Song_TryChartFile(song, candidate, streaming)) return 1;
        }
    }

    /* Common converted/imported chart filenames. */
    (void)snprintf(candidate, sizeof(candidate), "%s/data.json", directory);
    if (Song_TryChartFile(song, candidate, streaming)) return 1;
    (void)snprintf(candidate, sizeof(candidate), "%s/%s.json", directory, base);
    if (Song_TryChartFile(song, candidate, streaming)) return 1;

    files = LoadDirectoryFilesEx(directory, ".json", false);

    /* Prefer the requested difficulty among arbitrary Psych filenames. */
    if (diff != NULL && diff[0] != '\0') {
        char suffix[64];
        (void)snprintf(suffix, sizeof(suffix), "-%s.json", diff);
        for (unsigned i = 0; i < files.count; ++i) {
            if (Song_IsEventsFile(files.paths[i])) continue;
            if (Song_EndsWithI(Song_BaseName(files.paths[i]), suffix) &&
                Song_TryChartFile(song, files.paths[i], streaming)) {
                loaded = 1;
                break;
            }
        }
    }

    /* Then try non-difficulty charts (usually the normal/default chart). */
    if (!loaded) {
        for (unsigned i = 0; i < files.count; ++i) {
            if (Song_IsEventsFile(files.paths[i]) || Song_IsDifficultyFile(files.paths[i])) continue;
            if (Song_TryChartFile(song, files.paths[i], streaming)) {
                loaded = 1;
                break;
            }
        }
    }

    /* Last resort: any valid difficulty chart in the folder. */
    if (!loaded) {
        for (unsigned i = 0; i < files.count; ++i) {
            if (Song_IsEventsFile(files.paths[i])) continue;
            if (Song_TryChartFile(song, files.paths[i], streaming)) {
                loaded = 1;
                break;
            }
        }
    }

    UnloadDirectoryFiles(files);
    return loaded;
}

static int Song_FindChartPathInDirectory(const char *directory, const char *diff,
                                         char out[SONG_PATH_MAX])
{
    char candidate[SONG_PATH_MAX];
    const char *base;
    FilePathList files;

    if (directory == NULL || !DirectoryExists(directory)) return 0;
    base = Song_BaseName(directory);

    if (diff != NULL && diff[0] != '\0' && strcmp(diff, "normal") != 0) {
        (void)snprintf(candidate, sizeof(candidate), "%s/data-%s.json", directory, diff);
        if (FileExists(candidate)) goto found_candidate;
        (void)snprintf(candidate, sizeof(candidate), "%s/%s-%s.json", directory, base, diff);
        if (FileExists(candidate)) goto found_candidate;
        (void)snprintf(candidate, sizeof(candidate), "%s/%s.json", directory, diff);
        if (FileExists(candidate)) goto found_candidate;
    }

    (void)snprintf(candidate, sizeof(candidate), "%s/data.json", directory);
    if (FileExists(candidate)) goto found_candidate;
    (void)snprintf(candidate, sizeof(candidate), "%s/%s.json", directory, base);
    if (FileExists(candidate)) goto found_candidate;

    /* V-Slice: <song>-chart.json e o chart (prefere-o); <song>-metadata.json nunca e chart. */
    files = LoadDirectoryFilesEx(directory, ".json", false);
    candidate[0] = '\0';
    for (unsigned i = 0; i < files.count; ++i) {
        const char *fileBase = Song_BaseName(files.paths[i]);
        if (Song_IsEventsFile(files.paths[i]) || strstr(fileBase, "-metadata") != NULL) continue;
        if (strstr(fileBase, "-chart.json") != NULL) {
            Song_NormalizePath(files.paths[i], out, SONG_PATH_MAX);
            UnloadDirectoryFiles(files);
            return 1;
        }
        if (candidate[0] == '\0')
            (void)snprintf(candidate, sizeof(candidate), "%s", files.paths[i]);
    }
    if (candidate[0] != '\0') {
        Song_NormalizePath(candidate, out, SONG_PATH_MAX);
        UnloadDirectoryFiles(files);
        return 1;
    }
    UnloadDirectoryFiles(files);
    return 0;

found_candidate:
    Song_NormalizePath(candidate, out, SONG_PATH_MAX);
    return 1;
}

int Song_ResolveChart(const char *dirPath, const char *diff,
                             char out[SONG_PATH_MAX])
{
    char normalized[SONG_PATH_MAX];
    char modRoot[SONG_PATH_MAX];
    char songName[256];
    char directory[SONG_PATH_MAX];

    if (dirPath == NULL || dirPath[0] == '\0' || out == NULL) return 0;
    Song_NormalizePath(dirPath, normalized, sizeof(normalized));
    (void)snprintf(songName, sizeof(songName), "%s", Song_BaseName(normalized));

    /* 1) New/imported layout: chart copied beside Inst.ogg in songs/<song>/. */
    if (Song_FindChartPathInDirectory(normalized, diff, out)) return 1;

    /* 2) Original Psych mod layout: assets/mods/<mod>/data/<song>/. */
    Song_GetModRoot(normalized, modRoot, sizeof(modRoot));
    if (modRoot[0] != '\0') {
        if (snprintf(directory, sizeof(directory), "%s/data/%s", modRoot, songName) < (int)sizeof(directory) &&
            Song_FindChartPathInDirectory(directory, diff, out)) return 1;
    }

    /* 3) Alternate converted layouts inside the song folder. */
    if (Song_PathJoin(directory, sizeof(directory), normalized, "charts") &&
        Song_FindChartPathInDirectory(directory, diff, out)) return 1;
    if (Song_PathJoin(directory, sizeof(directory), normalized, "data") &&
        Song_FindChartPathInDirectory(directory, diff, out)) return 1;

    /* 4) Base-game Psych layout. */
    if (snprintf(directory, sizeof(directory), "assets/data/%s", songName) < (int)sizeof(directory) &&
        Song_FindChartPathInDirectory(directory, diff, out)) return 1;

    return 0;
}

int Song_TryLoadChart(const char *dirPath, const char *diff, Song *song, char streaming)
{
    char normalized[SONG_PATH_MAX];
    char modRoot[SONG_PATH_MAX];
    char songName[256];
    char directory[SONG_PATH_MAX];

    if (dirPath == NULL || song == NULL) return 0;
    Song_NormalizePath(dirPath, normalized, sizeof(normalized));
    (void)snprintf(songName, sizeof(songName), "%s", Song_BaseName(normalized));

    if (Song_TryLoadChartDirectory(song, normalized, diff, streaming)) return 1;

    Song_GetModRoot(normalized, modRoot, sizeof(modRoot));
    if (modRoot[0] != '\0') {
        if (snprintf(directory, sizeof(directory), "%s/data/%s", modRoot, songName) < (int)sizeof(directory) &&
            Song_TryLoadChartDirectory(song, directory, diff, streaming)) return 1;
    }

    if (Song_PathJoin(directory, sizeof(directory), normalized, "charts") &&
        Song_TryLoadChartDirectory(song, directory, diff, streaming)) return 1;
    if (Song_PathJoin(directory, sizeof(directory), normalized, "data") &&
        Song_TryLoadChartDirectory(song, directory, diff, streaming)) return 1;

    if (snprintf(directory, sizeof(directory), "assets/data/%s", songName) < (int)sizeof(directory) &&
        Song_TryLoadChartDirectory(song, directory, diff, streaming)) return 1;

    return 0;
}
