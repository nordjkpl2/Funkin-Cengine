/*
 * TextMeasure.c - medicao de texto com cache de um valor (ver TextMeasure.h).
 */

#include "rendering/TextMeasure.h"
#include <string.h>

Vector2 TextMeasure_Get(TextMeasure* c, Font font, const char* text, float size, float spacing) {
    const size_t length = strlen(text);
    if (length >= TEXT_MEASURE_CAP)
        return MeasureTextEx(font, text, size, spacing);

    if (c->valid && c->size == size && c->spacing == spacing &&
        c->fontId == font.texture.id && strcmp(c->text, text) == 0)
        return c->result;

    memcpy(c->text, text, length + 1);
    c->size = size;
    c->spacing = spacing;
    c->fontId = font.texture.id;
    c->result = MeasureTextEx(font, text, size, spacing);
    c->valid = true;
    return c->result;
}
