/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * StageJson.c - Leitura do <stage>.json (formato Psych): zoom, posicoes e camera.
 */

#include "logic/stage/StageJson.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

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
