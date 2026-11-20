/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongJson.c - parser do chart JSON (formato Psych/FNF, com ou sem o objeto "song").
 */
#include "data/Song.h"
#include "data/SongInternal.h"
#include "yyjson.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONG_JSON_MAX_BYTES (64L * 1024L * 1024L)

void Song_CopyString(char dst[SONG_NAME_MAX], const char *src, const char *fallback)
{
    const char *value = src != NULL ? src : fallback;
    size_t length;
    memset(dst, 0, SONG_NAME_MAX);
    if (value == NULL) return;
    length = strlen(value);
    if (length > SONG_NAME_MAX - 1u) length = SONG_NAME_MAX - 1u;
    memcpy(dst, value, length);
}

static int Song_ReadFile(const char *path, char **outBuffer, size_t *outSize)
{
    FILE *file;
    long size;
    char *buffer;

    if (outBuffer == NULL || outSize == NULL || path == NULL) return 0;
    *outBuffer = NULL;
    *outSize = 0;

    file = fopen(path, "rb");
    if (file == NULL) return 0;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    if (size <= 0 || size > SONG_JSON_MAX_BYTES || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    buffer = (char *)malloc((size_t)size + 1u);
    if (buffer == NULL) {
        fclose(file);
        return 0;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return 0;
    }
    fclose(file);
    buffer[size] = '\0';
    *outBuffer = buffer;
    *outSize = (size_t)size;
    return 1;
}

typedef struct RawNote {
    float time;
    int rawId;
    float len;
} RawNote;

static int RawNote_Compare(const void *left, const void *right)
{
    const float a = ((const RawNote *)left)->time;
    const float b = ((const RawNote *)right)->time;
    return a < b ? -1 : a > b ? 1 : 0;
}

static double Song_JsonNumber(yyjson_val *value, double fallback)
{
    if (!yyjson_is_num(value)) return fallback;
    if (yyjson_is_int(value)) return (double)yyjson_get_sint(value);
    if (yyjson_is_uint(value)) return (double)yyjson_get_uint(value);
    return yyjson_get_real(value);
}

static float Song_JsonFloat(yyjson_val *value, float fallback)
{
    double number = Song_JsonNumber(value, fallback);
    if (!isfinite(number) || number > FLT_MAX || number < -FLT_MAX) return fallback;
    return (float)number;
}

char Song_ParseJSON(Song *song, const char *jsonPath)
{
    char *buffer = NULL;
    size_t bufferSize = 0;
    yyjson_doc *doc = NULL;
    yyjson_val *root;
    yyjson_val *songObject;
    yyjson_val *sectionsValue;
    Song parsed;
    float topBpm;
    float speed;
    float runningBpm;
    size_t sectionCount;
    int success = 0;

    if (song == NULL || jsonPath == NULL) return 0;
    memset(&parsed, 0, sizeof(parsed));
    if (!Song_ReadFile(jsonPath, &buffer, &bufferSize)) return 0;

    doc = yyjson_read(buffer, bufferSize, 0);
    free(buffer);
    buffer = NULL;
    if (doc == NULL) return 0;

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) goto cleanup;
    songObject = yyjson_obj_get(root, "song");
    if (!yyjson_is_obj(songObject)) songObject = root;

    sectionsValue = yyjson_obj_get(songObject, "notes");
    if (!yyjson_is_arr(sectionsValue)) goto cleanup;
    sectionCount = yyjson_arr_size(sectionsValue);
    if (sectionCount == 0 || sectionCount > SONG_MAX_SECTIONS) goto cleanup;

    topBpm = Song_JsonFloat(yyjson_obj_get(songObject, "bpm"), 100.0f);
    if (topBpm < 10.0f) topBpm = 100.0f;
    speed = Song_JsonFloat(yyjson_obj_get(songObject, "speed"), 1.0f);
    if (speed < 0.1f) speed = 1.0f;

    Song_CopyString(parsed.player1,
        yyjson_is_str(yyjson_obj_get(songObject, "player1")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "player1")) : NULL, "bf");
    Song_CopyString(parsed.player2,
        yyjson_is_str(yyjson_obj_get(songObject, "player2")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "player2")) : NULL, "dad");
    Song_CopyString(parsed.stage,
        yyjson_is_str(yyjson_obj_get(songObject, "stage")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "stage")) : NULL, "stage");
    Song_CopyString(parsed.gfVersion,
        yyjson_is_str(yyjson_obj_get(songObject, "gfVersion")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "gfVersion")) : NULL, "gf");
    Song_CopyString(parsed.noteSkin,
        yyjson_is_str(yyjson_obj_get(songObject, "noteSkin")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "noteSkin")) : NULL, "");
    Song_CopyString(parsed.arrowSkin,
        yyjson_is_str(yyjson_obj_get(songObject, "arrowSkin")) ?
        yyjson_get_str(yyjson_obj_get(songObject, "arrowSkin")) : NULL, "");

    parsed.bpm = topBpm;
    parsed.speed = speed;
    parsed.sectionCount = (int)sectionCount;
    parsed.sections = (Section *)calloc(sectionCount, sizeof(Section));
    if (parsed.sections == NULL) goto cleanup;

    runningBpm = topBpm;
    for (size_t i = 0; i < sectionCount; ++i) {
        yyjson_val *sectionValue = yyjson_arr_get(sectionsValue, i);
        Section *section = &parsed.sections[i];
        yyjson_val *lengthValue;
        yyjson_val *mustHitValue;
        yyjson_val *sectionBpmValue;
        yyjson_val *changeBpmValue;
        yyjson_val *notesValue;
        size_t rawCount;
        RawNote *raw = NULL;
        DataNote *output = NULL;
        int valid = 0;
        int kept = 0;
        int sorted = 1;
        float lastTime = -1.0f;
        float lastLaneTime[8];

        section->len = 16;
        section->bpm = runningBpm;
        if (!yyjson_is_obj(sectionValue)) continue;

        lengthValue = yyjson_obj_get(sectionValue, "lengthInSteps");
        if (lengthValue == NULL) lengthValue = yyjson_obj_get(sectionValue, "lengthinsteps");
        mustHitValue = yyjson_obj_get(sectionValue, "mustHitSection");
        sectionBpmValue = yyjson_obj_get(sectionValue, "bpm");
        changeBpmValue = yyjson_obj_get(sectionValue, "changeBPM");
        notesValue = yyjson_obj_get(sectionValue, "sectionNotes");

        if (yyjson_is_num(lengthValue)) {
            double length = Song_JsonNumber(lengthValue, 16.0);
            if (length >= 1.0 && length <= INT_MAX) section->len = (int)length;
        }
        section->mustHit = (char)(yyjson_is_bool(mustHitValue) && yyjson_get_bool(mustHitValue));
        if (yyjson_is_bool(changeBpmValue) && yyjson_get_bool(changeBpmValue) &&
            yyjson_is_num(sectionBpmValue)) {
            float sectionBpm = Song_JsonFloat(sectionBpmValue, runningBpm);
            if (sectionBpm >= 10.0f) runningBpm = sectionBpm;
        }
        section->bpm = runningBpm;

        if (!yyjson_is_arr(notesValue)) continue;
        rawCount = yyjson_arr_size(notesValue);
        if (rawCount == 0 || rawCount > (size_t)INT_MAX ||
            rawCount > SIZE_MAX / sizeof(RawNote)) continue;

        raw = (RawNote *)malloc(sizeof(RawNote) * rawCount);
        if (raw == NULL) continue;

        for (size_t j = 0; j < rawCount; ++j) {
            yyjson_val *noteValue = yyjson_arr_get(notesValue, j);
            yyjson_val *timeValue;
            yyjson_val *laneValue;
            yyjson_val *lengthNoteValue;
            double laneNumber;
            double noteLength;
            float noteTime;
            int lane;

            if (!yyjson_is_arr(noteValue) || yyjson_arr_size(noteValue) < 2) continue;
            timeValue = yyjson_arr_get(noteValue, 0);
            laneValue = yyjson_arr_get(noteValue, 1);
            lengthNoteValue = yyjson_arr_size(noteValue) > 2 ? yyjson_arr_get(noteValue, 2) : NULL;
            if (!yyjson_is_num(timeValue) || !yyjson_is_num(laneValue)) continue;

            noteTime = Song_JsonFloat(timeValue, -1.0f);
            laneNumber = Song_JsonNumber(laneValue, -1.0);
            if (noteTime < 0.0f || !isfinite(laneNumber) ||
                laneNumber < (double)INT_MIN || laneNumber > (double)INT_MAX) continue;
            lane = (int)laneNumber % 8;
            if (lane < 0) lane += 8;

            noteLength = yyjson_is_num(lengthNoteValue) ?
                         Song_JsonNumber(lengthNoteValue, 0.0) : 0.0;
            if (!isfinite(noteLength) || noteLength < 0.0 || noteLength > FLT_MAX) continue;

            if (valid > 0 && noteTime < lastTime) sorted = 0;
            lastTime = noteTime;
            raw[valid].time = noteTime;
            raw[valid].rawId = lane;
            raw[valid].len = (float)noteLength;
            ++valid;
        }

        if (valid == 0) {
            free(raw);
            continue;
        }
        if (!sorted) qsort(raw, (size_t)valid, sizeof(*raw), RawNote_Compare);

        output = (DataNote *)malloc(sizeof(DataNote) * (size_t)valid);
        if (output == NULL) {
            free(raw);
            continue;
        }
        for (int k = 0; k < 8; ++k) lastLaneTime[k] = -999999.0f;

        for (int j = 0; j < valid; ++j) {
            int lane = raw[j].rawId;
            int id;
            if (raw[j].time - lastLaneTime[lane] < 2.0f) continue;
            lastLaneTime[lane] = raw[j].time;
            id = section->mustHit ?
                 (lane < 4 ? -(lane + 1) : lane - 3) :
                 (lane < 4 ? lane + 1 : -(lane - 3));
            output[kept].time = raw[j].time;
            output[kept].id = (int16_t)id;
            output[kept]._pad = 0;
            output[kept].len = raw[j].len;
            ++kept;
        }
        free(raw);

        if (kept == 0) {
            free(output);
            continue;
        }
        section->notes = output;
        section->noteCount = kept;
    }

    *song = parsed;
    memset(&parsed, 0, sizeof(parsed));
    success = 1;

cleanup:
    if (doc != NULL) yyjson_doc_free(doc);
    if (!success) Song_Free(&parsed);
    return (char)success;
}
