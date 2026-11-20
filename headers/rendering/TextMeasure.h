#pragma once
/*
 * TextMeasure.h - medicao de texto com cache de um valor.
 *
 * MeasureTextEx percorre o texto glifo a glifo. Em telas que medem o mesmo rotulo
 * a cada frame, o resultado e guardado aqui e so e refeito quando o texto, o tamanho
 * ou o espacamento mudam.
 */

#include "raylib.h"
#include <stdbool.h>

#define TEXT_MEASURE_CAP 128

typedef struct {
    char text[TEXT_MEASURE_CAP];
    float size;
    float spacing;
    unsigned int fontId;   // texture.id da fonte usada na ultima medicao
    Vector2 result;
    bool valid;
} TextMeasure;

// Mede text com a fonte, usando o cache de c. Texto maior que TEXT_MEASURE_CAP nao e guardado.
Vector2 TextMeasure_Get(TextMeasure* c, Font font, const char* text, float size, float spacing);
