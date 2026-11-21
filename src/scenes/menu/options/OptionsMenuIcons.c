/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuIcons.c - texto com sombra e icones/logos desenhados a mao das linhas e cabecalhos.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include <math.h>

/* Texto com sombra deslocada para cima/esquerda. */
void DrawOptionText(const char* text, float x, float y, int fontSize, Color fill, Color shadow) {
    DrawTextEx(mainFont, text, (Vector2) {x, y + TEXT_SHADOW}, fontSize, TEXT_SPACING, shadow);
    DrawTextEx(mainFont, text, (Vector2) {x - TEXT_SHADOW, y}, fontSize, TEXT_SPACING, fill);
}

/* Quatro quadrados (estilo logo do Windows), com a mesma sombra do texto. */
void DrawWindowLogo(float rowY, Color fill, Color shadow) {
    const float step = LOGO_SQUARE + LOGO_GAP;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;

        for (int i = 0; i < 4; i++) {
            const float x = windowLogoX + (i % 2) * step + shift;
            const float y = rowY + LOGO_TOP + (i / 2) * step + shift;
            DrawRectangleRec((Rectangle) {x, y, LOGO_SQUARE, LOGO_SQUARE}, color);
        }
    }
}

/* Circulo com a mesma sombra do texto. */
void DrawOtherLogo(float rowY, Color fill, Color shadow) {
    const float centerY = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f; // alinhado ao meio do logo de quadrados
    const float centerX = otherLogoX + LOGO_CIRCLE_RADIUS;

    DrawCircleV((Vector2) {centerX + TEXT_SHADOW, centerY + TEXT_SHADOW}, LOGO_CIRCLE_RADIUS, shadow);
    DrawCircleV((Vector2) {centerX, centerY}, LOGO_CIRCLE_RADIUS, fill);
}

/* Linha com pontas arredondadas. */
static void DrawRoundLine(Vector2 a, Vector2 b, Color color) {
    DrawLineEx(a, b, SYMBOL_STROKE, color);
    DrawCircleV(a, SYMBOL_STROKE * 0.5f, color);
    DrawCircleV(b, SYMBOL_STROKE * 0.5f, color);
}

/* X, quadrado, triangulo e bolinha em uma cor, deslocados por `shift`. */
static void DrawPlayStationSymbols(float rowY, float shift, Color color) {
    const float h = SYMBOL_HALF;
    const float step = 2.0f * h + SYMBOL_GAP;
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f + shift;
    float cx = gameplayLogoX + h + shift;

    // X
    DrawRoundLine((Vector2) {cx - h, cy - h}, (Vector2) {cx + h, cy + h}, color);
    DrawRoundLine((Vector2) {cx - h, cy + h}, (Vector2) {cx + h, cy - h}, color);

    // Quadrado
    cx += step;
    DrawRoundLine((Vector2) {cx - h, cy - h}, (Vector2) {cx + h, cy - h}, color);
    DrawRoundLine((Vector2) {cx + h, cy - h}, (Vector2) {cx + h, cy + h}, color);
    DrawRoundLine((Vector2) {cx + h, cy + h}, (Vector2) {cx - h, cy + h}, color);
    DrawRoundLine((Vector2) {cx - h, cy + h}, (Vector2) {cx - h, cy - h}, color);

    // Triangulo
    cx += step;
    const Vector2 top = {cx, cy - h - 2.0f};
    const Vector2 left = {cx - h - 2.0f, cy + h - 2.0f};
    const Vector2 right = {cx + h + 2.0f, cy + h - 2.0f};
    DrawRoundLine(top, left, color);
    DrawRoundLine(left, right, color);
    DrawRoundLine(right, top, color);

    // Bolinha
    cx += step;
    DrawRing((Vector2) {cx, cy}, h + 2.0f - SYMBOL_STROKE * 0.5f, h + 2.0f + SYMBOL_STROKE * 0.5f, 0.0f, 360.0f, 36, color);
}

void DrawGameplayLogo(float rowY, Color fill, Color shadow) {
    DrawPlayStationSymbols(rowY, TEXT_SHADOW, shadow);
    DrawPlayStationSymbols(rowY, 0.0f, fill);
}

/* Quadro (moldura com borda grossa), com a mesma sombra do texto. */
void DrawVisualLogo(float rowY, Color fill, Color shadow) {
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        DrawRectangleLinesEx((Rectangle) {visualLogoX + shift, cy - FRAME_HEIGHT * 0.5f + shift, FRAME_WIDTH, FRAME_HEIGHT},
                             FRAME_STROKE, color);
    }
}

/* Engrenagem (anel com dentes), com a mesma sombra do texto. */
void DrawRenderLogo(float rowY, Color fill, Color shadow) {
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f;
    const float cx0 = renderLogoX + GEAR_OUTER_R;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = cx0 + shift;
        const float y = cy + shift;

        DrawRing((Vector2) {cx, y}, GEAR_HOLE_R, GEAR_BODY_R, 0.0f, 360.0f, 48, color);
        for (int k = 0; k < GEAR_TEETH; k++) {
            const float angle = 2.0f * PI * (float)k / (float)GEAR_TEETH;
            const float d = GEAR_BODY_R + GEAR_TOOTH_LEN * 0.5f - 1.0f; // 1px de encaixe no corpo
            const Rectangle tooth = {cx + cosf(angle) * d, y + sinf(angle) * d, GEAR_TOOTH_LEN, GEAR_TOOTH_W};
            DrawRectanglePro(tooth, (Vector2) {GEAR_TOOTH_LEN * 0.5f, GEAR_TOOTH_W * 0.5f}, angle * RAD2DEG, color);
        }
    }
}

/* Nota musical (colcheia), com a mesma sombra do texto. */
void DrawMusicLogo(float rowY, Color fill, Color shadow) {
    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float top = rowY + LOGO_TOP + shift;
        const float headX = musicLogoX + NOTE_HEAD_RADIUS + shift;
        const float headY = top + NOTE_STEM_H;
        const float stemLeft = headX + NOTE_HEAD_RADIUS - NOTE_STEM_W;
        const float stemX = stemLeft + NOTE_STEM_W * 0.5f;

        DrawCircleV((Vector2) {headX, headY}, NOTE_HEAD_RADIUS, color);
        DrawRectangleRec((Rectangle) {stemLeft, top, NOTE_STEM_W, NOTE_STEM_H}, color);
        DrawRoundLine((Vector2) {stemX, top}, (Vector2) {stemX + NOTE_FLAG_W, top + NOTE_FLAG_H}, color);
    }
}

/* Setinha da secao: "v" aberta, ">" recolhida. */
void DrawChevron(float rowY, bool collapsed, Color fill, Color shadow) {
    const float s = CHEVRON_SIZE;
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const float shift = pass == 0 ? TEXT_SHADOW * 0.6f : 0.0f;
        const Color color = pass == 0 ? shadow : fill;
        const float cx = CHEVRON_X + shift;
        const float y = cy + shift;

        Vector2 a, b, c;
        if (collapsed) {
            a = (Vector2) {cx - s * 0.55f, y - s};
            b = (Vector2) {cx + s * 0.55f, y};
            c = (Vector2) {cx - s * 0.55f, y + s};
        } else {
            a = (Vector2) {cx - s, y - s * 0.55f};
            b = (Vector2) {cx, y + s * 0.55f};
            c = (Vector2) {cx + s, y - s * 0.55f};
        }

        DrawLineEx(a, b, CHEVRON_STROKE, color);
        DrawLineEx(b, c, CHEVRON_STROKE, color);
        DrawCircleV(a, CHEVRON_STROKE * 0.5f, color);
        DrawCircleV(b, CHEVRON_STROKE * 0.5f, color);
        DrawCircleV(c, CHEVRON_STROKE * 0.5f, color);
    }
}

/* TV (retangulo com antenas), com a mesma sombra do texto. */
void DrawTvIcon(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float left = TV_X + shift;
        const float top = cy - TV_H * 0.5f + shift;
        const float cx = left + TV_W * 0.5f;

        DrawLineEx((Vector2) {cx - 6.0f, top}, (Vector2) {cx - 14.0f, top - 12.0f}, TV_STROKE * 0.6f, color);
        DrawLineEx((Vector2) {cx + 6.0f, top}, (Vector2) {cx + 14.0f, top - 12.0f}, TV_STROKE * 0.6f, color);
        DrawRectangleLinesEx((Rectangle) {left, top, TV_W, TV_H}, TV_STROKE, color);
    }
}

/* Triangulo preenchido, independente da ordem dos vertices (DrawTriangle exige um sentido). */
static void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    const float cross = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (cross > 0.0f) {   // DrawTriangle do raylib so desenha no sentido oposto ao que ficava invisivel
        const Vector2 t = b;
        b = c;
        c = t;
    }
    DrawTriangle(a, b, c, color);
}

/* Coracao: curva parametrica classica, preenchida em leque a partir do centro. r = meia largura. */
static void DrawHeart(float cx, float cy, float r, Color color) {
    enum { HEART_SEGMENTS = 32 };
    static Vector2 unit[HEART_SEGMENTS]; // contorno normalizado (largura ~32), calculado uma vez
    static bool unitReady;

    if (!unitReady) {
        for (int i = 0; i < HEART_SEGMENTS; i++) {
            const float t = 2.0f * PI * (float)i / (float)HEART_SEGMENTS;
            const float s = sinf(t);
            const float x = 16.0f * s * s * s;
            const float y = 13.0f * cosf(t) - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t);
            unit[i] = (Vector2) {x, -(y + 2.5f)}; // +2.5 centraliza verticalmente
        }
        unitReady = true;
    }

    const float scale = r / 9.0f;
    Vector2 pts[HEART_SEGMENTS];
    for (int i = 0; i < HEART_SEGMENTS; i++)
        pts[i] = (Vector2) {cx + unit[i].x * scale, cy + unit[i].y * scale};

    for (int i = 0; i < HEART_SEGMENTS; i++)
        FillTriangle((Vector2) {cx, cy}, pts[i], pts[(i + 1) % HEART_SEGMENTS], color);
}

/* Adaga na vertical: ponta para baixo, cruzando o coracao (cx, cy = centro da lamina). */
static void DrawDagger(float cx, float cy, Color color) {
    FillTriangle((Vector2) {cx - 5.0f, cy - 8.0f}, (Vector2) {cx + 5.0f, cy - 8.0f}, (Vector2) {cx, cy + 12.0f}, color); // lamina
    DrawRectangleRec((Rectangle) {cx - 12.0f, cy - 13.0f, 24.0f, 5.0f}, color);  // guarda
    DrawRectangleRec((Rectangle) {cx - 3.0f, cy - 19.0f, 6.0f, 7.0f}, color);    // cabo
    DrawCircleV((Vector2) {cx, cy - 22.0f}, 4.0f, color);                       // pomo
}

/* Coracao com adaga cravada, com a mesma sombra do texto. */
void DrawHealthIcons(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = HEALTH_ICON_CX + shift;

        DrawHeart(cx, cy + 3.0f + shift, HEALTH_HEART_R, color);
        DrawDagger(cx, cy + shift, color);
    }
}

/* Uma seta (triangulo apontando para a direita), com a mesma sombra do texto. */
void DrawKeyArrow(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;
    const float s = KEY_ARROW_HALF;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = KEY_ARROW_CX + shift;
        const float y = cy + shift;

        FillTriangle((Vector2) {cx - s * 0.5f, y - s}, (Vector2) {cx - s * 0.5f, y + s}, (Vector2) {cx + s * 0.8f, y}, color);
    }
}

/* Seta dupla <-->, para Stretch. */
static void DrawStretchIcon(float cx, float cy, Color color) {
    const float h = STRETCH_HALF;
    const float head = STRETCH_HEAD;

    DrawRoundLine((Vector2) {cx - h, cy}, (Vector2) {cx + h, cy}, color);
    FillTriangle((Vector2) {cx - h - 4.0f, cy}, (Vector2) {cx - h + head, cy - head * 0.7f}, (Vector2) {cx - h + head, cy + head * 0.7f}, color);
    FillTriangle((Vector2) {cx + h + 4.0f, cy}, (Vector2) {cx + h - head, cy - head * 0.7f}, (Vector2) {cx + h - head, cy + head * 0.7f}, color);
}

/* Caixa com um "T" dentro, para as opcoes de texto (Show Score / Score Position). */
static void DrawTextIcon(float cx, float cy, Color color) {
    const float h = TEXT_ICON_HALF;
    DrawRectangleLinesEx((Rectangle) {cx - h, cy - h, h * 2.0f, h * 2.0f}, 4.0f, color);
    DrawTextEx(mainFont, "T", (Vector2) {cx - 8.0f, cy - 17.0f}, 34, 0.0f, color);
}

/* Duas bolinhas, uma um pouco mais alta que a outra (Bopping Icons). */
static void DrawBopIcon(float cx, float cy, Color color) {
    DrawCircleV((Vector2) {cx - 9.0f, cy + 3.0f}, BOP_ICON_R, color);
    DrawCircleV((Vector2) {cx + 9.0f, cy - 3.0f}, BOP_ICON_R, color);
}

/* Quadrado, para Keys Monitor. */
static void DrawSquareIcon(float cx, float cy, Color color) {
    const float h = KEYMON_ICON_HALF;
    DrawRectangleLinesEx((Rectangle) {cx - h, cy - h, h * 2.0f, h * 2.0f}, 5.0f, color);
}

/* Icone da linha (so para as opcoes que tem), com a mesma sombra do texto. */
void DrawExtraIcon(int i, float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    // Setas de nota reais da skin (sem sombra propria: a nota ja tem contorno).
    if (i == OPTION_SCROLL_STYLE) {   // Normal = cima, Downscroll = baixo, Middlescroll = as duas
        const int v = (int)options[OPTION_SCROLL_STYLE].value;
        if (v == 0) NoteColors_DrawNoteIcon(2, EXTRA_ICON_CX, cy, 0.2f);
        else if (v == 1) NoteColors_DrawNoteIcon(1, EXTRA_ICON_CX, cy, 0.2f);
        else {
            NoteColors_DrawNoteIcon(2, EXTRA_ICON_CX, cy - 16.0f, 0.16f);
            NoteColors_DrawNoteIcon(1, EXTRA_ICON_CX, cy + 16.0f, 0.16f);
        }
        return;
    }
    if (i == OPTION_STRUM_BG) {       // as 4 notas lado a lado, na cor de cada coluna
        for (int k = 0; k < 4; k++)
            NoteColors_DrawNoteIcon(k, 26.0f + 32.0f * (float)k, cy, 0.2f);
        return;
    }

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = EXTRA_ICON_CX + shift;
        const float y = cy + shift;

        switch (i) {
            case OPTION_FILL_SCREEN:    DrawStretchIcon(cx, y, color); break;
            case OPTION_SHOW_SCORE:
            case OPTION_SCORE_POSITION: DrawTextIcon(cx, y, color); break;
            case OPTION_BOPPING_ICONS:  DrawBopIcon(cx, y, color); break;
            case OPTION_STRUM_MOBILE:   DrawSquareIcon(cx, y, color); break;
            default: return;
        }
    }
}
