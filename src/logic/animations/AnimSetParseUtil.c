/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetParseUtil.c - Helpers de parse compartilhados pelos loaders de animset (XML Sparrow/Codename, yxml).
 */

#include "std.h"
#include "logic/animations/AnimSetParseUtil.h"
#include "Log.h"
#include "yxml.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


bool AnimParse_YxmlFile(const char *path, YxmlTokenHandler handler, void *user)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "[AnimationSets] cannot open XML: %s\n", path);
        return false;
    }

    yxml_t xml;
    unsigned char xmlStack[XML_STACK_SIZE];
    unsigned char input[XML_READ_CHUNK];
    yxml_init(&xml, xmlStack, sizeof(xmlStack));

    bool ok = true;
    size_t bytesRead;

    while ((bytesRead = fread(input, 1, sizeof(input), file)) != 0 && ok) {
        for (size_t i = 0; i < bytesRead; ++i) {
            yxml_ret_t token = yxml_parse(&xml, input[i]);
            if (token < 0) {
                fprintf(stderr,
                    "[AnimationSets] XML parse error in %s at line %u, byte %llu (code %d)\n",
                    path,
                    (unsigned)xml.line,
                    (unsigned long long)xml.byte,
                    (int)token
                );
                ok = false;
                break;
            }

            if (handler && !handler(&xml, token, user)) {
                fprintf(stderr, "[AnimationSets] invalid XML data in %s at line %u\n",
                    path, (unsigned)xml.line);
                ok = false;
                break;
            }
        }
    }

    if (ok && ferror(file)) {
        fprintf(stderr, "[AnimationSets] read error: %s\n", path);
        ok = false;
    }

    if (ok && yxml_eof(&xml) < 0) {
        fprintf(stderr, "[AnimationSets] incomplete XML document: %s\n", path);
        ok = false;
    }

    fclose(file);
    return ok;
}

bool AnimParse_Int32(const char *text, int32_t *out)
{
    if (!text || !*text || !out) return false;

    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (errno || end == text || *end != '\0' || value < INT32_MIN || value > INT32_MAX)
        return false;

    *out = (int32_t)value;
    return true;
}

bool AnimParse_Float(const char *text, float *out)
{
    if (!text || !*text || !out) return false;

    errno = 0;
    char *end = NULL;
    float value = strtof(text, &end);

    if (errno || end == text || *end != '\0' || !isfinite(value))
        return false;

    *out = value;
    return true;
}

bool AnimParse_Bool(const char *text, bool *out)
{
    if (!text || !out) return false;

    if (!strcmp(text, "true") || !strcmp(text, "1") || !strcmp(text, "yes")) {
        *out = true;
        return true;
    }

    if (!strcmp(text, "false") || !strcmp(text, "0") || !strcmp(text, "no")) {
        *out = false;
        return true;
    }

    return false;
}

bool AnimParse_Color(const char *text, Color *out)
{
    if (!text || !out) return false;

    if (*text == '#') ++text;
    else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;

    size_t length = strlen(text);
    if (length != 6 && length != 8) return false;

    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 16);
    if (errno || end == text || *end != '\0') return false;

    if (length == 6) {
        out->r = (unsigned char)((value >> 16) & 0xFFu);
        out->g = (unsigned char)((value >> 8) & 0xFFu);
        out->b = (unsigned char)(value & 0xFFu);
        out->a = 255;
    } else {
        out->r = (unsigned char)((value >> 24) & 0xFFu);
        out->g = (unsigned char)((value >> 16) & 0xFFu);
        out->b = (unsigned char)((value >> 8) & 0xFFu);
        out->a = (unsigned char)(value & 0xFFu);
    }

    return true;
}

bool AnimParse_AppendAttr(char *buffer, size_t capacity, size_t *length, const char *data)
{
    if (!buffer || !capacity || !length || !data) return false;

    size_t add = strlen(data);
    if (add > capacity - 1u - *length) return false;

    memcpy(buffer + *length, data, add);
    *length += add;
    buffer[*length] = '\0';
    return true;
}

bool AnimParse_EndsWithXML(const char *path)
{
    const char *extension = strrchr(path, '.');
    if (!extension) return false;
    if (strlen(extension) != 4u) return false;
    return tolower((unsigned char)extension[1]) == 'x' &&
           tolower((unsigned char)extension[2]) == 'm' &&
           tolower((unsigned char)extension[3]) == 'l';
}

bool AnimParse_IsLikelyXML(const char *path)
{
    if (!path) return false;

    if (AnimParse_EndsWithXML(path))
        return true;

    FILE *file = fopen(path, "rb");
    if (!file) return false;

    unsigned char probe[256];
    size_t count = fread(probe, 1, sizeof(probe), file);
    fclose(file);

    size_t i = 0;
    if (count >= 3 && probe[0] == 0xEF && probe[1] == 0xBB && probe[2] == 0xBF)
        i = 3;

    while (i < count && isspace(probe[i])) ++i;
    return i < count && probe[i] == '<';
}

void AnimationSet_FreeArrays(RayAnimationHandler *animations)
{
    if (!animations) return;
    free(animations->frameArena);
    free(animations->animations);
    animations->frameArena = NULL;
    animations->animations = NULL;
    animations->animationCount = 0;
}
