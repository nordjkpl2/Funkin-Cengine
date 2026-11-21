/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * ModImportParse.c - Parse de JSON/chart/week/personagem, Sparrow XML e escrita do .animset.
 */

#include "std.h"
#include "data/modimport/ModImport.h"
#include "data/modimport/ModImportInternal.h"
#include "data/modimport/ModImportParse.h"
#include "data/modimport/ModImportPath.h"
#include "data/modimport/ModImportFiles.h"

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

/* JSON                                                                       */
/* ------------------------------------------------------------------------- */

static const char *MI_JsonString(yyjson_val *object, const char *key)
{
    yyjson_val *value;
    if (object == NULL || !yyjson_is_obj(object)) return NULL;
    value = yyjson_obj_get(object, key);
    if (!yyjson_is_str(value)) return NULL;
    return yyjson_get_str(value);
}

void MI_AddName(MI_NameList *list, const char *name)
{
    char candidate[64];
    size_t len;
    if (list == NULL || name == NULL || !MI_IsSafeComponent(name)) return;
    len = strlen(name);
    if (len == 0 || len >= sizeof(candidate)) return;
    memcpy(candidate, name, len + 1);

    for (int i = 0; i < list->count; ++i) {
        if (MI_CaseEqual(list->names[i], candidate)) return;
    }
    if (list->count < MI_MAX_ICON_NAMES) {
        memcpy(list->names[list->count], candidate, len + 1);
        ++list->count;
    }
}

void MI_CollectChartActors(yyjson_val *root, MI_NameList *names)
{
    yyjson_val *song;
    static const char *keys[] = {"player1", "player2"};

    if (root == NULL) return;
    song = yyjson_obj_get(root, "song");
    if (!yyjson_is_obj(song)) song = root;

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const char *value = MI_JsonString(song, keys[i]);
        MI_AddName(names, value);
    }
}

void MI_ParseWeekIcons(const char *weekPath, MI_NameList *names)
{
    long size = 0;
    char *buffer = MI_ReadFile(weekPath, &size, 2L * 1024L * 1024L);
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *songs;
    yyjson_val *weekCharacters;

    if (buffer == NULL) return;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return;

    root = yyjson_doc_get_root(doc);
    songs = yyjson_obj_get(root, "songs");
    if (yyjson_is_arr(songs)) {
        size_t i, max;
        yyjson_val *row;
        yyjson_arr_foreach(songs, i, max, row) {
            if (!yyjson_is_arr(row) || yyjson_arr_size(row) < 2) continue;
            {
                yyjson_val *icon = yyjson_arr_get(row, 1);
                if (yyjson_is_str(icon)) MI_AddName(names, yyjson_get_str(icon));
            }
        }
    }

    weekCharacters = yyjson_obj_get(root, "weekCharacters");
    if (yyjson_is_arr(weekCharacters)) {
        size_t i, max;
        yyjson_val *value;
        yyjson_arr_foreach(weekCharacters, i, max, value) {
            if (yyjson_is_str(value)) MI_AddName(names, yyjson_get_str(value));
        }
    }
    yyjson_doc_free(doc);
}

int MI_ParseCharacterJson(const char *jsonPath, const char *fallbackName,
                                 MI_CharInfo *out)
{
    long size = 0;
    char *buffer;
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *animations;
    const char *healthicon;
    const char *image;

    if (out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    if (fallbackName != NULL) {
        MI_CopyString(out->healthicon, sizeof(out->healthicon), fallbackName);
        (void)snprintf(out->imagePath, sizeof(out->imagePath), "characters/%s", fallbackName);
    }

    buffer = MI_ReadFile(jsonPath, &size, 1024L * 1024L);
    if (buffer == NULL) return 0;
    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL) return 0;
    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return 0;
    }

    healthicon = MI_JsonString(root, "healthicon");
    if (healthicon != NULL && MI_IsSafeComponent(healthicon))
        (void)snprintf(out->healthicon, sizeof(out->healthicon), "%s", healthicon);

    image = MI_JsonString(root, "image");
    if (image != NULL && image[0] != '\0' && strstr(image, "..") == NULL &&
        strchr(image, '\\') == NULL) {
        const char *normalized = image;
        size_t len;
        if (strncmp(normalized, "assets/", 7) == 0) normalized += 7;
        if (strncmp(normalized, "images/", 7) == 0) normalized += 7;
        len = strlen(normalized);
        if (len < sizeof(out->imagePath)) {
            memcpy(out->imagePath, normalized, len + 1);
            {
                char *dot = strrchr(out->imagePath, '.');
                if (dot != NULL) *dot = '\0';
            }
        }
    }

    animations = yyjson_obj_get(root, "animations");
    if (yyjson_is_arr(animations)) {
        size_t i, max;
        yyjson_val *entry;
        yyjson_arr_foreach(animations, i, max, entry) {
            const char *role;
            const char *prefix;
            MI_CharAnim *anim;
            yyjson_val *indices;
            if (out->animCount >= MI_MAX_CHAR_ANIMS) {
                TraceLog(LOG_WARNING, "IMPORT: %s: mais de %d animacoes, o resto foi ignorado",
                         jsonPath, MI_MAX_CHAR_ANIMS);
                break;
            }
            if (!yyjson_is_obj(entry)) break;
            role = MI_JsonString(entry, "anim");
            prefix = MI_JsonString(entry, "name");
            if (role == NULL || role[0] == '\0' || prefix == NULL || prefix[0] == '\0') continue;
            if (strlen(prefix) >= sizeof(out->anims[0].prefix) ||
                strlen(role) >= sizeof(out->anims[0].role)) continue;

            anim = &out->anims[out->animCount];
            (void)snprintf(anim->role, sizeof(anim->role), "%s", role);
            (void)snprintf(anim->prefix, sizeof(anim->prefix), "%s", prefix);
            indices = yyjson_obj_get(entry, "indices");
            if (yyjson_is_arr(indices)) {
                size_t k, kmax;
                yyjson_val *indexValue;
                yyjson_arr_foreach(indices, k, kmax, indexValue) {
                    if (anim->indexCount >= MI_MAX_ANIM_FRAMES) {
                        TraceLog(LOG_WARNING, "IMPORT: %s: animacao '%s' cortada em %d indices",
                                 jsonPath, role, MI_MAX_ANIM_FRAMES);
                        break;
                    }
                    if (yyjson_is_int(indexValue)) {
                        int64_t index = yyjson_get_int(indexValue);
                        if (index >= 0 && index <= INT_MAX)
                            anim->indices[anim->indexCount++] = (int32_t)index;
                    }
                }
            }
            ++out->animCount;
        }
    }

    yyjson_doc_free(doc);
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Sparrow XML -> .animset                                                    */
/* ------------------------------------------------------------------------- */

static int MI_ParseInteger(const char *text, int32_t *out)
{
    char *end;
    long value;
    if (text == NULL || text[0] == '\0' || out == NULL) return 0;
    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE ||
        value < INT32_MIN || value > INT32_MAX) return 0;
    *out = (int32_t)value;
    return 1;
}

static int MI_XmlAttribute(const char *tagStart, const char *tagEnd,
                           const char *attribute, char *out, size_t outSize)
{
    const char *p = tagStart;
    size_t attrLen;
    if (out == NULL || outSize == 0 || tagStart == NULL || tagEnd == NULL ||
        attribute == NULL) return 0;
    out[0] = '\0';
    attrLen = strlen(attribute);

    while (p < tagEnd && *p != '<') ++p;
    if (p < tagEnd) ++p;
    while (p < tagEnd && !isspace((unsigned char)*p) && *p != '>') ++p;

    while (p < tagEnd) {
        const char *nameStart;
        size_t nameLen;
        char quote;
        size_t valueLen;

        while (p < tagEnd && (isspace((unsigned char)*p) || *p == '/')) ++p;
        if (p >= tagEnd || *p == '>') break;
        nameStart = p;
        while (p < tagEnd && !isspace((unsigned char)*p) && *p != '=' && *p != '>') ++p;
        nameLen = (size_t)(p - nameStart);
        while (p < tagEnd && isspace((unsigned char)*p)) ++p;
        if (p >= tagEnd || *p != '=') {
            while (p < tagEnd && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
            continue;
        }
        ++p;
        while (p < tagEnd && isspace((unsigned char)*p)) ++p;
        if (p >= tagEnd || (*p != '\'' && *p != '"')) continue;
        quote = *p++;
        {
            const char *valueStart = p;
            while (p < tagEnd && *p != quote) ++p;
            valueLen = (size_t)(p - valueStart);
            if (nameLen == attrLen && memcmp(nameStart, attribute, attrLen) == 0) {
                if (valueLen >= outSize) valueLen = outSize - 1;
                memcpy(out, valueStart, valueLen);
                out[valueLen] = '\0';
                return valueLen > 0;
            }
        }
        if (p < tagEnd) ++p;
    }
    return 0;
}

static int MI_ParseXmlFrames(const char *xml, MI_XmlFrame *frames, int capacity)
{
    const char *p = xml;
    int count = 0;
    if (xml == NULL || frames == NULL || capacity <= 0) return 0;

    while (count < capacity) {
        const char *tag = strstr(p, "<SubTexture");
        const char *tagEnd;
        char value[128];
        MI_XmlFrame frame;

        if (tag == NULL) break;
        if (!(isspace((unsigned char)tag[11]) || tag[11] == '>' || tag[11] == '/')) {
            p = tag + 11;
            continue;
        }
        tagEnd = strchr(tag, '>');
        if (tagEnd == NULL) break;
        memset(&frame, 0, sizeof(frame));

        if (!MI_XmlAttribute(tag, tagEnd, "name", frame.name, sizeof(frame.name)) ||
            !MI_XmlAttribute(tag, tagEnd, "x", value, sizeof(value)) ||
            !MI_ParseInteger(value, &frame.x) ||
            !MI_XmlAttribute(tag, tagEnd, "y", value, sizeof(value)) ||
            !MI_ParseInteger(value, &frame.y) ||
            !MI_XmlAttribute(tag, tagEnd, "width", value, sizeof(value)) ||
            !MI_ParseInteger(value, &frame.w) ||
            !MI_XmlAttribute(tag, tagEnd, "height", value, sizeof(value)) ||
            !MI_ParseInteger(value, &frame.h) || frame.w <= 0 || frame.h <= 0) {
            p = tagEnd + 1;
            continue;
        }

        if (MI_XmlAttribute(tag, tagEnd, "frameX", value, sizeof(value)))
            (void)MI_ParseInteger(value, &frame.fx);
        if (MI_XmlAttribute(tag, tagEnd, "frameY", value, sizeof(value)))
            (void)MI_ParseInteger(value, &frame.fy);

        frames[count++] = frame;
        p = tagEnd + 1;
    }
    return count;
}

static int MI_FrameSuffix(const char *frameName, const char *prefix, uint64_t *order)
{
    const char *suffix;
    uint64_t value = 0;
    if (strncmp(frameName, prefix, strlen(prefix)) != 0) return 0;
    suffix = frameName + strlen(prefix);
    if (*suffix == '\0') {
        *order = 0;
        return 1;
    }
    if (*suffix == '.') ++suffix;
    if (*suffix == '\0') return 0;
    for (; *suffix; ++suffix) {
        unsigned digit;
        if (*suffix < '0' || *suffix > '9') return 0;
        digit = (unsigned)(*suffix - '0');
        if (value > (UINT64_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    *order = value;
    return 1;
}

static int MI_CompareFrameCandidate(const void *left, const void *right)
{
    const MI_FrameCandidate *a = (const MI_FrameCandidate *)left;
    const MI_FrameCandidate *b = (const MI_FrameCandidate *)right;
    if (a->order < b->order) return -1;
    if (a->order > b->order) return 1;
    return a->frameIndex < b->frameIndex ? -1 : a->frameIndex > b->frameIndex;
}

static int MI_WriteExact(FILE *file, const void *data, size_t size)
{
    return size == 0 || fwrite(data, 1, size, file) == size;
}

int MI_WriteAnimset(const MI_CharInfo *info, const char *xmlPath,
                           const char *animsetPath)
{
    long xmlSize = 0;
    char *xml = NULL;
    MI_XmlFrame *frames = NULL;
    MI_FrameCandidate *candidates = NULL;
    int32_t *selected = NULL;
    int *counts = NULL;
    int frameCount;
    int kept = 0;
    int written = 0;
    char tempPath[MI_PATH_MAX] = {0};
    FILE *out = NULL;
    int ok = 0;

    if (info == NULL || info->animCount <= 0 || xmlPath == NULL || animsetPath == NULL)
        return 0;
    xml = MI_ReadFile(xmlPath, &xmlSize, 16L * 1024L * 1024L);
    if (xml == NULL) goto cleanup;

    frames = (MI_XmlFrame *)malloc(sizeof(*frames) * MI_MAX_XML_FRAMES);
    if (frames == NULL) goto cleanup;
    frameCount = MI_ParseXmlFrames(xml, frames, MI_MAX_XML_FRAMES);
    if (frameCount <= 0) goto cleanup;
    if (frameCount >= MI_MAX_XML_FRAMES)
        TraceLog(LOG_WARNING, "IMPORT: %s: atlas com %d frames ou mais, o resto foi ignorado",
                 xmlPath, MI_MAX_XML_FRAMES);

    candidates = (MI_FrameCandidate *)malloc(sizeof(*candidates) * (size_t)frameCount);
    selected = (int32_t *)malloc(sizeof(*selected) *
               (size_t)MI_MAX_CHAR_ANIMS * MI_MAX_ANIM_FRAMES);
    counts = (int *)calloc((size_t)MI_MAX_CHAR_ANIMS, sizeof(*counts));
    if (candidates == NULL || selected == NULL || counts == NULL) goto cleanup;

    for (int a = 0; a < info->animCount; ++a) {
        const MI_CharAnim *anim = &info->anims[a];
        int candidateCount = 0;
        int outputCount = 0;

        for (int f = 0; f < frameCount; ++f) {
            uint64_t order;
            if (MI_FrameSuffix(frames[f].name, anim->prefix, &order)) {
                candidates[candidateCount].frameIndex = f;
                candidates[candidateCount].order = order;
                ++candidateCount;
            }
        }
        if (candidateCount == 0) continue;
        qsort(candidates, (size_t)candidateCount, sizeof(*candidates),
              MI_CompareFrameCandidate);

        if (anim->indexCount > 0) {
            for (int k = 0; k < anim->indexCount && outputCount < MI_MAX_ANIM_FRAMES; ++k) {
                int index = anim->indices[k];
                if (index >= 0 && index < candidateCount)
                    selected[(size_t)a * MI_MAX_ANIM_FRAMES + outputCount++] =
                        candidates[index].frameIndex;
            }
        } else {
            int take = candidateCount < MI_MAX_ANIM_FRAMES ? candidateCount : MI_MAX_ANIM_FRAMES;
            if (candidateCount > take)
                TraceLog(LOG_WARNING, "IMPORT: %s: animacao '%s' cortada em %d de %d frames",
                         animsetPath, anim->role, take, candidateCount);
            for (int k = 0; k < take; ++k)
                selected[(size_t)a * MI_MAX_ANIM_FRAMES + outputCount++] =
                    candidates[k].frameIndex;
        }
        counts[a] = outputCount;
        if (outputCount > 0) ++kept;
    }
    if (kept == 0) goto cleanup;

    if (snprintf(tempPath, sizeof(tempPath), "%s.tmp", animsetPath) >= (int)sizeof(tempPath))
        goto cleanup;
    if (!MI_EnsureParent(tempPath)) goto cleanup;
    out = fopen(tempPath, "wb");
    if (out == NULL) goto cleanup;

    {
        int32_t animationCount = (int32_t)kept;
        if (!MI_WriteExact(out, &animationCount, sizeof(animationCount))) goto cleanup;
    }

    for (int a = 0; a < info->animCount; ++a) {
        const MI_CharAnim *anim = &info->anims[a];
        int32_t nameLength;
        int32_t dataCount;
        if (counts[a] <= 0) continue;
        nameLength = (int32_t)strlen(anim->prefix);
        dataCount = (int32_t)(counts[a] * 6);
        if (!MI_WriteExact(out, &nameLength, sizeof(nameLength)) ||
            !MI_WriteExact(out, anim->prefix, (size_t)nameLength) ||
            !MI_WriteExact(out, &dataCount, sizeof(dataCount))) goto cleanup;

        for (int k = 0; k < counts[a]; ++k) {
            const MI_XmlFrame *frame = &frames[
                selected[(size_t)a * MI_MAX_ANIM_FRAMES + k]];
            int32_t packed[6] = {
                frame->x, frame->y, frame->w, frame->h, frame->fx, frame->fy
            };
            if (!MI_WriteExact(out, packed, sizeof(packed))) goto cleanup;
        }
        ++written;
    }

    if (ferror(out)) goto cleanup;
    if (fclose(out) != 0) {
        out = NULL;
        goto cleanup;
    }
    out = NULL;

    (void)remove(animsetPath);
    if (rename(tempPath, animsetPath) != 0) goto cleanup;
    ok = written;

cleanup:
    if (out != NULL) fclose(out);
    if (!ok && tempPath[0] != '\0') (void)remove(tempPath);
    free(counts);
    free(selected);
    free(candidates);
    free(frames);
    free(xml);
    return ok;
}

/* ------------------------------------------------------------------------- */
