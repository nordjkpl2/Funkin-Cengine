/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterJson.c - Leitura do JSON do personagem (Psych): escala, camera, cores e definicoes de animacao.
 */

#include "characters/CharacterInternal.h"
#include "characters/CharacterJson.h"
#include "yyjson.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Personagem animset + JSON (Psych)                                         */
/* ------------------------------------------------------------------------- */


/* yyjson_get_real devolve 0 para inteiros: aceita int e real. */
static bool Char_JsonNumber(yyjson_val *value, double *out)
{
    if (yyjson_is_real(value)) *out = yyjson_get_real(value);
    else if (yyjson_is_sint(value)) *out = (double)yyjson_get_sint(value);
    else if (yyjson_is_uint(value)) *out = (double)yyjson_get_uint(value);
    else return false;
    return *out == *out; /* descarta NaN */
}

/* Le [x, y]; so mexe no que for numero. */
static void Char_JsonVec2(yyjson_val *array, float *x, float *y)
{
    double number;
    if (!yyjson_is_arr(array) || yyjson_arr_size(array) < 2u)
        return;
    if (Char_JsonNumber(yyjson_arr_get(array, 0), &number)) *x = (float)number;
    if (Char_JsonNumber(yyjson_arr_get(array, 1), &number)) *y = (float)number;
}

static void Char_JsonReadColor(yyjson_val *array, CharJson *json)
{
    if (!yyjson_is_arr(array) || yyjson_arr_size(array) < 3u)
        return;

    double channel[3];
    for (size_t i = 0; i < 3u; ++i) {
        if (!Char_JsonNumber(yyjson_arr_get(array, i), &channel[i]))
            return;
        if (channel[i] < 0.0) channel[i] = 0.0;
        if (channel[i] > 255.0) channel[i] = 255.0;
    }
    json->hasHealthColor = true;
    json->healthColor = (Color){ (unsigned char)channel[0], (unsigned char)channel[1],
                                 (unsigned char)channel[2], 255 };
}

static bool Char_JsonReadDef(yyjson_val *entry, CharAnimDef *def)
{
    if (!yyjson_is_obj(entry))
        return false;

    yyjson_val *role = yyjson_obj_get(entry, "anim");
    yyjson_val *sprite = yyjson_obj_get(entry, "name");
    if (!yyjson_is_str(role) || !yyjson_is_str(sprite) ||
        !yyjson_get_str(role)[0] || !yyjson_get_str(sprite)[0]) {
        return false;
    }

    Char_CopyString(def->role, sizeof(def->role), yyjson_get_str(role));
    Char_CopyString(def->sprite, sizeof(def->sprite), yyjson_get_str(sprite));
    def->fps = 24;
    def->looped = false;
    def->offsetX = 0.0f;
    def->offsetY = 0.0f;

    double fps;
    if (Char_JsonNumber(yyjson_obj_get(entry, "fps"), &fps) && fps >= 1.0 && fps <= 1000.0)
        def->fps = (int)fps;

    yyjson_val *loop = yyjson_obj_get(entry, "loop");
    if (yyjson_is_bool(loop))
        def->looped = yyjson_get_bool(loop);

    Char_JsonVec2(yyjson_obj_get(entry, "offsets"), &def->offsetX, &def->offsetY);
    return true;
}

void Char_JsonInit(CharJson *json)
{
    *json = (CharJson){ .scale = 1.0f, .singDuration = 4.0f,
                        .healthColor = { 255, 255, 255, 255 } };
}

/* Preenche so o que o JSON define; json deve ter passado por Char_JsonInit. */
bool Char_ReadJson(const char *path, CharJson *json)
{
    int size = GetFileLength(path);
    if (size <= 0 || size > CHAR_JSON_MAX_SIZE)
        return false;

    yyjson_doc *document = yyjson_read_file(path, 0, NULL, NULL);
    if (!document)
        return false;

    yyjson_val *root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(document);
        return false;
    }

    double number;
    if (Char_JsonNumber(yyjson_obj_get(root, "scale"), &number) && number > 0.05 && number < 8.0)
        json->scale = (float)number;
    if (Char_JsonNumber(yyjson_obj_get(root, "sing_duration"), &number) && number > 0.0 && number < 100.0)
        json->singDuration = (float)number;

    yyjson_val *flip = yyjson_obj_get(root, "flip_x");
    if (yyjson_is_bool(flip))
        json->flipX = yyjson_get_bool(flip);

    Char_JsonVec2(yyjson_obj_get(root, "camera_position"), &json->camera.x, &json->camera.y);
    Char_JsonVec2(yyjson_obj_get(root, "position"), &json->position.x, &json->position.y);
    Char_JsonReadColor(yyjson_obj_get(root, "healthbar_colors"), json);

    yyjson_val *icon = yyjson_obj_get(root, "healthicon");
    if (yyjson_is_str(icon) && Char_IsSafeName(yyjson_get_str(icon)))
        Char_CopyString(json->healthIcon, sizeof(json->healthIcon), yyjson_get_str(icon));

    yyjson_val *image = yyjson_obj_get(root, "image");
    if (yyjson_is_str(image) && yyjson_get_str(image)[0]) {
        char stem[128];
        Char_StemName(yyjson_get_str(image), stem, sizeof(stem));
        if (Char_IsSafeName(stem))
            Char_CopyString(json->imageBase, sizeof(json->imageBase), stem);
    }

    size_t index, max;
    yyjson_val *entry;
    yyjson_arr_foreach(yyjson_obj_get(root, "animations"), index, max, entry) {
        if (json->defCount >= CHAR_MAX_DEFS)
            break;
        if (Char_JsonReadDef(entry, &json->defs[json->defCount]))
            ++json->defCount;
    }

    yyjson_doc_free(document);
    return true;
}

/* Sem JSON (ou sem animacoes nele): o atlas usa os proprios nomes dos papeis. */
void Char_AddDefaultDefs(CharJson *json)
{
    static const char *const roles[] = { "idle", "singLEFT", "singDOWN", "singUP", "singRIGHT" };

    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i) {
        CharAnimDef *def = &json->defs[json->defCount++];
        Char_CopyString(def->role, sizeof(def->role), roles[i]);
        Char_CopyString(def->sprite, sizeof(def->sprite), roles[i]);
        def->fps = 24;
        def->looped = false;
        def->offsetX = 0.0f;
        def->offsetY = 0.0f;
    }
}

