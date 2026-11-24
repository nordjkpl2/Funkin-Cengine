/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * Song.c - API de carregamento de musica (chart JSON ou binario data.song).
 * O parser JSON esta em SongJson.c e a busca de arquivos em SongChartFind.c.
 *
 * Supported layouts:
 *   assets/songs/<song>/...
 *   assets/mods/<mod>/songs/<song>/...
 *   assets/mods/<mod>/data/<song>/...       (original Psych layout)
 *   assets/mods/mods/<mod>/...              (legacy duplicated root)
 */

#include "std.h"
#include "data/Song.h"
#include "data/SongInternal.h"
#include "data/SongStream.h"
#include "Log.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void File_ReadString(FILE *file, char str[SONG_NAME_MAX])
{
    int32_t length;
    if (fread(&length, sizeof(length), 1, file) != 1 || length < 1 || length > SONG_NAME_MAX - 1)
        Log_Fatal("invalid song string");
    if (fread(str, 1, (size_t)length, file) != (size_t)length)
        Log_Fatal("truncated song string");
    str[length] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Binary song loader                                                        */
/* ------------------------------------------------------------------------- */

void Song_Parse(Song *song, const char *songDataPath)
{
    FILE *file;
    int32_t sectionCount;

    if (song == NULL || songDataPath == NULL) Log_Fatal("invalid song path");
    memset(song, 0, sizeof(*song));
    file = fopen(songDataPath, "rb");
    if (file == NULL) {
        Log_WarnF("json not found: %s", songDataPath);
        return;
    }

    File_ReadString(file, song->player1);
    File_ReadString(file, song->player2);
    File_ReadString(file, song->stage);
    Song_CopyString(song->gfVersion, NULL, "gf");

    if (fread(&song->speed, sizeof(float), 1, file) != 1 ||
        fread(&sectionCount, sizeof(sectionCount), 1, file) != 1) {
        fclose(file);
        Log_Fatal("truncated binary song header");
    }
    if (!isfinite(song->speed) || song->speed <= 0.0f ||
        sectionCount < 1 || sectionCount > SONG_MAX_SECTIONS) {
        fclose(file);
        Log_Fatal("invalid binary song metadata");
    }

    song->sections = (Section *)calloc((size_t)sectionCount, sizeof(Section));
    if (song->sections == NULL) {
        fclose(file);
        Log_Fatal("out of memory loading binary song");
    }
    song->sectionCount = sectionCount;

    for (int32_t i = 0; i < sectionCount; ++i) {
        Section *section = &song->sections[i];
        int32_t noteCount;
        if (fread(&section->len, sizeof(section->len), 1, file) != 1 ||
            fread(&section->bpm, sizeof(section->bpm), 1, file) != 1 ||
            fread(&section->mustHit, 1, 1, file) != 1 ||
            fread(&noteCount, sizeof(noteCount), 1, file) != 1) {
            fclose(file);
            Log_Fatal("truncated binary song section");
        }
        if (section->len < 1 || !isfinite(section->bpm) || section->bpm < 10.0f ||
            noteCount < 0 || (size_t)noteCount > SIZE_MAX / sizeof(DataNote)) {
            fclose(file);
            Log_Fatal("invalid binary song section");
        }
        section->noteCount = noteCount;
        if (noteCount > 0) {
            section->notes = (DataNote *)malloc(sizeof(DataNote) * (size_t)noteCount);
            if (section->notes == NULL ||
                fread(section->notes, sizeof(DataNote), (size_t)noteCount, file) != (size_t)noteCount) {
                fclose(file);
                Log_Fatal("truncated binary song notes");
            }
        }
        for (int32_t j = 0; j < noteCount; ++j) {
            const DataNote *note = &section->notes[j];
            int id = abs((int)note->id);
            if (!isfinite(note->time) || note->time < 0.0f || id < 1 || id > 4 ||
                !isfinite(note->len) || note->len < 0.0f) {
                fclose(file);
                Log_Fatal("invalid binary song note");
            }
        }
    }
    fclose(file);
}

void Song_ReleaseNotes(Song *song)
{
    if (song == NULL || song->sections == NULL) return;
    for (int i = 0; i < song->sectionCount; ++i) {
        free(song->sections[i].notes);
        song->sections[i].notes = NULL;
        song->sections[i].noteCount = 0;
    }
}

void Song_Free(Song *song)
{
    if (song == NULL) return;
    if (song->stream != NULL) SongStream_Close(song->stream);
    if (song->sections != NULL) {
        for (int i = 0; i < song->sectionCount; ++i)
            free(song->sections[i].notes);
        free(song->sections);
    }
    memset(song, 0, sizeof(*song));
}

static int Song_TryLoadBinary(const char *dirPath, Song *song)
{
    char path[SONG_PATH_MAX];
    char normalized[SONG_PATH_MAX];
    char modRoot[SONG_PATH_MAX];
    char songName[256];

    if (dirPath == NULL || song == NULL) return 0;
    if (snprintf(path, sizeof(path), "%s/data.song", dirPath) < (int)sizeof(path) &&
        FileExists(path)) {
        Song_Parse(song, path);
        return 1;
    }

    Song_NormalizePath(dirPath, normalized, sizeof(normalized));
    Song_GetModRoot(normalized, modRoot, sizeof(modRoot));
    if (modRoot[0] != '\0') {
        (void)snprintf(songName, sizeof(songName), "%s", Song_BaseName(normalized));
        if (snprintf(path, sizeof(path), "%s/data/%s/data.song", modRoot, songName) < (int)sizeof(path) &&
            FileExists(path)) {
            Song_Parse(song, path);
            return 1;
        }
    }
    return 0;
}

void Song_LoadSongFromDir(Song *song, const char *dirPath, char streaming)
{
    if (song == NULL || dirPath == NULL) Log_Fatal("invalid song directory");
    memset(song, 0, sizeof(*song));
    if (Song_TryLoadChart(dirPath, "", song, streaming)) return;
    if (Song_TryLoadBinary(dirPath, song)) return;
    Log_WarnF("json not found: %s", dirPath);
}

void Song_LoadSongFromDirDiff(Song *song, const char *dirPath, const char *diff, char streaming)
{
    if (song == NULL || dirPath == NULL) Log_Fatal("invalid song directory");
    memset(song, 0, sizeof(*song));
    if (Song_TryLoadChart(dirPath, diff, song, streaming)) return;
    if (Song_TryLoadBinary(dirPath, song)) return;
    Log_WarnF("json not found: %s (difficulty: %s)",
              dirPath, diff != NULL ? diff : "default");
}

char Song_HasChart(const char *dirPath)
{
    char path[SONG_PATH_MAX];
    char normalized[SONG_PATH_MAX];
    char modRoot[SONG_PATH_MAX];
    char songName[256];
    if (dirPath == NULL || dirPath[0] == '\0') return 0;
    if (Song_ResolveChart(dirPath, "", path)) return 1;
    if (snprintf(path, sizeof(path), "%s/data.song", dirPath) < (int)sizeof(path) &&
        FileExists(path)) return 1;

    Song_NormalizePath(dirPath, normalized, sizeof(normalized));
    Song_GetModRoot(normalized, modRoot, sizeof(modRoot));
    if (modRoot[0] != '\0') {
        (void)snprintf(songName, sizeof(songName), "%s", Song_BaseName(normalized));
        if (snprintf(path, sizeof(path), "%s/data/%s/data.song", modRoot, songName) < (int)sizeof(path) &&
            FileExists(path)) return 1;
    }
    return 0;
}
