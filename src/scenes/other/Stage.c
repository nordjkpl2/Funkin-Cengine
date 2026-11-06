/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Stage.c - Stage Lua discovery, lightweight parsing and background fitting (Optimized & Fixed).
 */

#include "std.h"
#include "scenes/Stage.h"
#include "scenes/ModPath.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

#define STAGE_PATH_MAX 300
#define STAGE_MOD_ROOT_MAX 256
#define STAGE_TAG_MAX 64

static inline bool Stage_IsSafeName(const char *name)
{
    if (name == NULL || name[0] == '\0' || strstr(name, "..") != NULL)
        return false;

    for (const char *p = name; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\')
            return false;
    }
    return true;
}

/* Ordem de busca: mod/stages, mod/data/stages, assets/stages,
 * assets/data/stages, songDir. Serve para .lua e .json. */
static char Stage_FindFile(const char *stageName, const char *songDir,
                           const char *ext, char out[STAGE_PATH_MAX])
{
    char modRoot[STAGE_MOD_ROOT_MAX];
    char relative[STAGE_PATH_MAX];

    if (out == NULL)
        return 0;

    out[0] = '\0';

    if (!Stage_IsSafeName(stageName) || strcmp(stageName, "stage") == 0)
        return 0;

    if (!ModPath_Root(songDir, modRoot, sizeof(modRoot)))
        modRoot[0] = '\0';

    snprintf(relative, sizeof(relative), "stages/%s.%s", stageName, ext);
    if (ModPath_FindIn(modRoot, relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "data/stages/%s.%s", stageName, ext);
    if (ModPath_FindIn(modRoot, relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "stages/%s.%s", stageName, ext);
    if (ModPath_FindIn("assets", relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "data/stages/%s.%s", stageName, ext);
    if (ModPath_FindIn("assets", relative, out, STAGE_PATH_MAX))
        return 1;

    snprintf(relative, sizeof(relative), "%s.%s", stageName, ext);
    return ModPath_FindIn(songDir, relative, out, STAGE_PATH_MAX) ? 1 : 0;
}

char Stage_FindLua(const char *stageName, const char *songDir, char out[300])
{
    return Stage_FindFile(stageName, songDir, "lua", out);
}

char Stage_FindJson(const char *stageName, const char *songDir, char out[300])
{
    return Stage_FindFile(stageName, songDir, "json", out);
}

/* ------------------------------------------------------------------------- */
/* Stage JSON (Psych): defaultZoom, isPixelStage, boyfriend/girlfriend/opponent */
/* ------------------------------------------------------------------------- */

#define STAGE_JSON_MAX (256L * 1024L)

/* yyjson_get_real devolve 0 para inteiros; aqui aceitamos int e real. */
static bool Stage_JsonNumber(yyjson_val *value, float *out)
{
    double number;

    if (yyjson_is_real(value)) number = yyjson_get_real(value);
    else if (yyjson_is_sint(value)) number = (double)yyjson_get_sint(value);
    else if (yyjson_is_uint(value)) number = (double)yyjson_get_uint(value);
    else return false;

    if (!isfinite(number) || fabs(number) > 1.0e9)
        return false;

    *out = (float)number;
    return true;
}

static void Stage_JsonPair(yyjson_val *root, const char *key, float out[2])
{
    yyjson_val *array = yyjson_obj_get(root, key);
    float x, y;

    if (!yyjson_is_arr(array) || yyjson_arr_size(array) < 2)
        return;

    if (Stage_JsonNumber(yyjson_arr_get(array, 0), &x) &&
        Stage_JsonNumber(yyjson_arr_get(array, 1), &y)) {
        out[0] = x;
        out[1] = y;
    }
}

char Stage_LoadJson(const char *path, StageMeta *out)
{
    FILE *file;
    long size;
    char *buffer;
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *value;
    float zoom;

    if (out == NULL)
        return 0;

    memset(out, 0, sizeof(*out));
    out->defaultZoom = 1.0f;
    out->cameraSpeed = 1.0f;
    out->boyfriend[0] = 770.0f;  out->boyfriend[1] = 100.0f;
    out->girlfriend[0] = 400.0f; out->girlfriend[1] = 130.0f;
    out->opponent[0] = 100.0f;   out->opponent[1] = 100.0f;

    if (path == NULL || path[0] == '\0')
        return 0;

    file = fopen(path, "rb");
    if (file == NULL)
        return 0;

    if (fseek(file, 0, SEEK_END) != 0 ||
        (size = ftell(file)) <= 0 || size > STAGE_JSON_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
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

    doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (doc == NULL)
        return 0;

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return 0;
    }

    value = yyjson_obj_get(root, "defaultZoom");
    if (Stage_JsonNumber(value, &zoom) && zoom > 0.05f && zoom < 10.0f)
        out->defaultZoom = zoom;

    value = yyjson_obj_get(root, "isPixelStage");
    out->isPixelStage = (yyjson_is_bool(value) && yyjson_get_bool(value)) ? 1 : 0;

    value = yyjson_obj_get(root, "hide_girlfriend");
    out->hideGirlfriend = (yyjson_is_bool(value) && yyjson_get_bool(value)) ? 1 : 0;

    Stage_JsonPair(root, "boyfriend", out->boyfriend);
    Stage_JsonPair(root, "girlfriend", out->girlfriend);
    Stage_JsonPair(root, "opponent", out->opponent);
    Stage_JsonPair(root, "camera_boyfriend", out->cameraBoyfriend);
    Stage_JsonPair(root, "camera_opponent", out->cameraOpponent);

    value = yyjson_obj_get(root, "camera_speed");
    if (Stage_JsonNumber(value, &zoom) && zoom > 0.0 && zoom < 20.0)
        out->cameraSpeed = (float)zoom;

    yyjson_doc_free(doc);
    return 1;
}

typedef struct StageTagMap {
    char tag[STAGE_TAG_MAX];
    int index;
} StageTagMap;

typedef enum StageLuaCall {
    LUA_CALL_NONE = 0,
    LUA_CALL_MAKE_SPRITE,
    LUA_CALL_MAKE_ANIMATED_SPRITE,
    LUA_CALL_MAKE_GRAPHIC,
    LUA_CALL_SET_SCROLL,
    LUA_CALL_SET_SCROLL_SHORT,
    LUA_CALL_SET_SCALE,
    LUA_CALL_SET_SCALE_LUA,
    LUA_CALL_ADD_SPRITE,
    LUA_CALL_SET_OBJECT_ORDER,
    LUA_CALL_ADD_ANIM_PREFIX,
    LUA_CALL_GLITCH,
    LUA_CALL_IGNORED_ANIMATION,
    LUA_CALL_UPDATE
} StageLuaCall;

static inline bool Stage_TokenEquals(const char *token, size_t tokenLength, const char *literal)
{
    const size_t literalLength = strlen(literal);
    return tokenLength == literalLength && memcmp(token, literal, literalLength) == 0;
}

/* Tabela por nome: nao depende de contar letras. Roda so no load do stage. */
static StageLuaCall Stage_GetCall(const char *token, size_t length)
{
    static const struct { const char *name; StageLuaCall call; } calls[] = {
        { "makeLuaSprite",            LUA_CALL_MAKE_SPRITE },
        { "makeAnimatedLuaSprite",    LUA_CALL_MAKE_ANIMATED_SPRITE },
        { "makeGraphic",              LUA_CALL_MAKE_GRAPHIC },
        { "setLuaSpriteScrollFactor", LUA_CALL_SET_SCROLL },
        { "setScrollFactor",          LUA_CALL_SET_SCROLL_SHORT },
        { "scaleObject",              LUA_CALL_SET_SCALE },
        { "scaleLuaSprite",           LUA_CALL_SET_SCALE_LUA },
        { "addLuaSprite",             LUA_CALL_ADD_SPRITE },
        { "setObjectOrder",           LUA_CALL_SET_OBJECT_ORDER },
        { "addAnimationByPrefix",     LUA_CALL_ADD_ANIM_PREFIX },
        { "addGlitchEffect",          LUA_CALL_GLITCH },
        { "setProperty",              LUA_CALL_IGNORED_ANIMATION },
        { "onUpdate",                 LUA_CALL_UPDATE },
    };

    for (size_t i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
        if (Stage_TokenEquals(token, length, calls[i].name))
            return calls[i].call;
    }
    return LUA_CALL_NONE;
}

static const char *Stage_SkipQuotedString(const char *p)
{
    const char quote = *p++;
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p++ == quote)
            break;
    }
    return p;
}

static const char *Stage_SkipLongBracket(const char *p)
{
    if (*p != '[')
        return p;

    const char *q = p + 1;
    while (*q == '=')
        ++q;

    if (*q != '[')
        return p;

    const size_t equalsCount = (size_t)(q - (p + 1));
    p = q + 1;

    while (*p != '\0') {
        if (*p == ']') {
            const char *close = p + 1;
            size_t i = 0;
            while (i < equalsCount && close[i] == '=')
                ++i;
            if (i == equalsCount && close[i] == ']')
                return close + i + 1;
        }
        ++p;
    }
    return p;
}

static inline const char *Stage_SkipComment(const char *p)
{
    const char *longText = Stage_SkipLongBracket(p + 2);
    if (longText != p + 2)
        return longText;

    while (*p != '\0' && *p != '\n')
        ++p;
    return p;
}

static inline const char *Stage_SkipArgumentSeparators(const char *p, const char *end)
{
    while (p < end && (isspace((unsigned char)*p) || *p == ','))
        ++p;
    return p;
}

static const char *Stage_FindCallEnd(const char *openParen)
{
    unsigned depth = 1;
    const char *p = openParen + 1;

    while (*p != '\0') {
        if (*p == '\'' || *p == '"') {
            p = Stage_SkipQuotedString(p);
            continue;
        }
        if (p[0] == '-' && p[1] == '-') {
            p = Stage_SkipComment(p);
            continue;
        }
        if (*p == '[') {
            const char *afterLong = Stage_SkipLongBracket(p);
            if (afterLong != p) {
                p = afterLong;
                continue;
            }
        }
        if (*p == '(') {
            ++depth;
        } else if (*p == ')') {
            if (--depth == 0)
                return p + 1;
        }
        ++p;
    }
    return NULL;
}

static bool Stage_ParseStringArg(const char **cursor, const char *end, char *out, size_t outCapacity)
{
    const char *p = Stage_SkipArgumentSeparators(*cursor, end);
    if (p >= end || (*p != '\'' && *p != '"') || outCapacity == 0)
        return false;

    const char quote = *p++;
    size_t written = 0;
    bool overflow = false;
    bool closed = false;

    while (p < end && *p != '\0') {
        char c = *p++;
        if (c == quote) {
            closed = true;
            break;
        }
        if (c == '\\' && p < end) {
            c = *p++;
            switch (c) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default: break;
            }
        }
        if (written + 1 < outCapacity)
            out[written++] = c;
        else
            overflow = true;
    }

    out[written] = '\0';
    *cursor = p;
    return closed && !overflow && written > 0;
}

static bool Stage_ParseFloatArg(const char **cursor, const char *end, float *out)
{
    const char *p = Stage_SkipArgumentSeparators(*cursor, end);
    if (p >= end)
        return false;

    char *numberEnd = NULL;
    const float value = strtof(p, &numberEnd);
    if (numberEnd == p || numberEnd > end || !isfinite(value))
        return false;

    *out = value;
    *cursor = numberEnd;
    return true;
}

static bool Stage_ParseIntArg(const char **cursor, const char *end, int *out)
{
    const char *p = Stage_SkipArgumentSeparators(*cursor, end);
    if (p >= end)
        return false;

    errno = 0;
    char *numberEnd = NULL;
    const long value = strtol(p, &numberEnd, 10);
    if (numberEnd == p || numberEnd > end || errno == ERANGE || value < INT_MIN || value > INT_MAX)
        return false;

    *out = (int)value;
    *cursor = numberEnd;
    return true;
}

static bool Stage_ParseBoolArg(const char **cursor, const char *end, bool *out)
{
    const char *p = Stage_SkipArgumentSeparators(*cursor, end);
    if (p >= end)
        return false;

    const char *start = p;
    while (p < end && (isalpha((unsigned char)*p) || *p == '_'))
        ++p;

    if (Stage_TokenEquals(start, (size_t)(p - start), "true")) {
        *out = true;
        *cursor = p;
        return true;
    }
    if (Stage_TokenEquals(start, (size_t)(p - start), "false")) {
        *out = false;
        *cursor = p;
        return true;
    }
    return false;
}

/* Função auxiliar restaurada para parsear strings de tags */
static inline bool Stage_ParseTagArg(const char **args, const char *argsEnd, char tag[STAGE_TAG_MAX])
{
    return Stage_ParseStringArg(args, argsEnd, tag, STAGE_TAG_MAX);
}

static int Stage_FindTag(const StageTagMap *maps, int mapCount, const char *tag)
{
    for (int i = 0; i < mapCount; ++i) {
        if (strcmp(maps[i].tag, tag) == 0)
            return i;
    }
    return -1;
}

static int Stage_GetOrCreateSprite(StageTagMap *maps, int *mapCount, StageSprite *out, int *objectCount, int capacity, const char *tag)
{
    const int mapIndex = Stage_FindTag(maps, *mapCount, tag);
    if (mapIndex >= 0)
        return maps[mapIndex].index;

    if (*objectCount >= capacity || *objectCount >= STAGE_MAX_SPRITES || *mapCount >= STAGE_MAX_SPRITES)
        return -1;

    const int index = (*objectCount)++;
    StageSprite *sprite = &out[index];
    memset(sprite, 0, sizeof(*sprite));

    sprite->scrollX = sprite->scrollY = sprite->scaleX = sprite->scaleY = 1.0f;
    sprite->colorR = sprite->colorG = sprite->colorB = sprite->colorA = 255;
    sprite->isGraphic = 0;

    snprintf(maps[*mapCount].tag, sizeof(maps[*mapCount].tag), "%s", tag);
    maps[*mapCount].index = index;
    ++(*mapCount);

    return index;
}

static inline int Stage_HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool Stage_ParseColor(const char *text, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *alpha)
{
    if (text == NULL)
        return false;

    if (text[0] == '#') {
        ++text;
    } else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    const size_t length = strlen(text);
    if (length != 6 && length != 8)
        return false;

    uint8_t channels[4] = {0, 0, 0, 255};
    const size_t channelCount = length / 2;

    for (size_t i = 0; i < channelCount; ++i) {
        const int hi = Stage_HexDigit(text[i * 2]);
        const int lo = Stage_HexDigit(text[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        channels[i] = (uint8_t)((hi << 4) | lo);
    }

    *red = channels[0];
    *green = channels[1];
    *blue = channels[2];
    *alpha = channels[3];
    return true;
}

int Stage_ParseLua(const char *luaText, StageSprite *out, int capacity, int *ignoredGlitch, int *ignoredAnim)
{
    if (ignoredGlitch != NULL) *ignoredGlitch = 0;
    if (ignoredAnim != NULL) *ignoredAnim = 0;

    if (luaText == NULL || out == NULL || capacity <= 0)
        return 0;

    if (capacity > STAGE_MAX_SPRITES)
        capacity = STAGE_MAX_SPRITES;

    StageTagMap maps[STAGE_MAX_SPRITES];
    int mapCount = 0, objectCount = 0, glitchCount = 0, ignoredAnimCount = 0;
    const char *p = luaText;

    while (*p != '\0') {
        if (isspace((unsigned char)*p)) {
            ++p;
            continue;
        }
        if (p[0] == '-' && p[1] == '-') {
            p = Stage_SkipComment(p);
            continue;
        }
        if (*p == '\'' || *p == '"') {
            p = Stage_SkipQuotedString(p);
            continue;
        }
        if (*p == '[') {
            const char *afterLong = Stage_SkipLongBracket(p);
            if (afterLong != p) {
                p = afterLong;
                continue;
            }
        }

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *token = p++;
            while (isalnum((unsigned char)*p) || *p == '_')
                ++p;

            const StageLuaCall call = Stage_GetCall(token, (size_t)(p - token));
            if (call == LUA_CALL_NONE)
                continue;

            const char *openParen = p;
            while (*openParen != '\0' && isspace((unsigned char)*openParen))
                ++openParen;
            if (*openParen != '(') {
                p = openParen;
                continue;
            }

            const char *afterCall = Stage_FindCallEnd(openParen);
            if (afterCall == NULL)
                break;

            const char *args = openParen + 1;
            const char *argsEnd = afterCall - 1;
            char tag[STAGE_TAG_MAX] = {0};
            int index = -1;

            switch (call) {
                case LUA_CALL_MAKE_SPRITE:
                case LUA_CALL_MAKE_ANIMATED_SPRITE: {
                    char image[128] = {0};
                    float x = 0.0f, y = 0.0f;

                    if (Stage_ParseTagArg(&args, argsEnd, tag) &&
                        Stage_ParseStringArg(&args, argsEnd, image, sizeof(image)) &&
                        Stage_ParseFloatArg(&args, argsEnd, &x) &&
                        Stage_ParseFloatArg(&args, argsEnd, &y)) {
                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0) {
                            snprintf(out[index].image, sizeof(out[index].image), "%s", image);
                            out[index].x = x;
                            out[index].y = y;
                            out[index].isGraphic = 0;
                            out[index].isAnimated = (call == LUA_CALL_MAKE_ANIMATED_SPRITE) ? 1 : 0;
                            out[index].graphicWidth = out[index].graphicHeight = 0.0f;
                        }
                    }
                } break;

                case LUA_CALL_MAKE_GRAPHIC: {
                    float width = 0.0f, height = 0.0f;
                    char colorText[24] = {0};
                    uint8_t red = 255, green = 255, blue = 255, alpha = 255;

                    if (Stage_ParseTagArg(&args, argsEnd, tag) &&
                        Stage_ParseFloatArg(&args, argsEnd, &width) &&
                        Stage_ParseFloatArg(&args, argsEnd, &height) &&
                        width >= 0.0f && height >= 0.0f) {
                        if (Stage_ParseStringArg(&args, argsEnd, colorText, sizeof(colorText))) {
                            (void)Stage_ParseColor(colorText, &red, &green, &blue, &alpha);
                        }

                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0) {
                            StageSprite *sprite = &out[index];
                            sprite->image[0] = '\0';
                            sprite->x = sprite->y = 0.0f;
                            sprite->graphicWidth = width;
                            sprite->graphicHeight = height;
                            sprite->colorR = red;
                            sprite->colorG = green;
                            sprite->colorB = blue;
                            sprite->colorA = alpha;
                            sprite->isGraphic = 1;
                        }
                    }
                } break;

                case LUA_CALL_SET_SCROLL:
                case LUA_CALL_SET_SCROLL_SHORT:
                case LUA_CALL_SET_SCALE:
                case LUA_CALL_SET_SCALE_LUA: {
                    float x = 1.0f, y = 1.0f;

                    if (Stage_ParseTagArg(&args, argsEnd, tag) &&
                        Stage_ParseFloatArg(&args, argsEnd, &x)) {
                        if (!Stage_ParseFloatArg(&args, argsEnd, &y))
                            y = x;

                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0) {
                            if (call == LUA_CALL_SET_SCROLL || call == LUA_CALL_SET_SCROLL_SHORT) {
                                out[index].scrollX = x;
                                out[index].scrollY = y;
                            } else {
                                out[index].scaleX = x;
                                out[index].scaleY = y;
                            }
                        }
                    }
                } break;

                case LUA_CALL_ADD_SPRITE: {
                    bool front = false;
                    if (Stage_ParseTagArg(&args, argsEnd, tag)) {
                        (void)Stage_ParseBoolArg(&args, argsEnd, &front);
                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0)
                            out[index].front = front ? 1 : 0;
                    }
                } break;

                case LUA_CALL_SET_OBJECT_ORDER: {
                    int order = 0;
                    if (Stage_ParseTagArg(&args, argsEnd, tag) &&
                        Stage_ParseIntArg(&args, argsEnd, &order)) {
                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0) {
                            /* Psych: o numero e o indice na lista do PlayState:
                               [fixos, sprites de fundo..., gf, dad, bf, frente...].
                               Numero baixo fica atras dos personagens; so passa para
                               a frente depois dos tres grupos (fundos + 4). */
                            out[index].front = order >= objectCount + 3 ? 1 : 0;
                        }
                    }
                } break;

                case LUA_CALL_ADD_ANIM_PREFIX: {
                    /* addAnimationByPrefix(tag, nome, prefixo, fps, loop): guarda so a primeira. */
                    char animName[64] = {0};
                    char prefix[64] = {0};
                    float fps = 24.0f;
                    bool loop = false;

                    if (Stage_ParseTagArg(&args, argsEnd, tag) &&
                        Stage_ParseStringArg(&args, argsEnd, animName, sizeof(animName)) &&
                        Stage_ParseStringArg(&args, argsEnd, prefix, sizeof(prefix))) {
                        (void)Stage_ParseFloatArg(&args, argsEnd, &fps);
                        (void)Stage_ParseBoolArg(&args, argsEnd, &loop);

                        index = Stage_GetOrCreateSprite(maps, &mapCount, out, &objectCount, capacity, tag);
                        if (index >= 0 && out[index].animPrefix[0] == '\0') {
                            snprintf(out[index].animPrefix, sizeof(out[index].animPrefix), "%s", prefix);
                            out[index].animFps = fps > 0.0f ? fps : 24.0f;
                            out[index].animLoop = loop ? 1 : 0;
                        }
                    }
                } break;

                case LUA_CALL_GLITCH:
                    ++glitchCount;
                    break;

                case LUA_CALL_IGNORED_ANIMATION:
                case LUA_CALL_UPDATE:
                    ++ignoredAnimCount;
                    break;

                default:
                    break;
            }

            p = afterCall;
            continue;
        }

        ++p;
    }

    int outputCount = 0;
    for (int i = 0; i < objectCount; ++i) {
        if (out[i].image[0] == '\0' && !out[i].isGraphic)
            continue;

        if (outputCount != i) {
            out[outputCount] = out[i];
        }
        ++outputCount;
    }

    if (ignoredGlitch != NULL) *ignoredGlitch = glitchCount;
    if (ignoredAnim != NULL) *ignoredAnim = ignoredAnimCount;

    return outputCount;
}

