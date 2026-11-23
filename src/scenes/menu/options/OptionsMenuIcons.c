/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuIcons.c - texto com sombra e icones/logos desenhados a mao das linhas e cabecalhos.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "GameData.h"
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
    const float cy = rowY + ROW_FONT_SIZE * 0.5f + ROW_ICON_DY;

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
    const float cy = rowY + ROW_FONT_SIZE * 0.5f + ROW_ICON_DY;

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
    const float cy = rowY + ROW_FONT_SIZE * 0.5f + ROW_ICON_DY;
    const float s = KEY_ARROW_HALF;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = KEY_ARROW_CX + shift;
        const float y = cy + shift;

        FillTriangle((Vector2) {cx - s * 0.5f, y - s}, (Vector2) {cx - s * 0.5f, y + s}, (Vector2) {cx + s * 0.8f, y}, color);
    }
}

/* Quatro cantos com as pontas para fora (expandir ate a borda) + bloco central, para Stretch. */
static void DrawStretchIcon(float cx, float cy, Color color) {
    const float w = STRETCH_HALF, h = STRETCH_HALF * 0.7f, len = 9.0f;

    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            const float x = cx + sx * w, y = cy + sy * h;
            DrawRoundLine((Vector2) {x - sx * len, y}, (Vector2) {x, y}, color);
            DrawRoundLine((Vector2) {x, y - sy * len}, (Vector2) {x, y}, color);
        }
    DrawRectangleRec((Rectangle) {cx - 6.0f, cy - 4.0f, 12.0f, 8.0f}, color);
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

/* Teclado (corpo com teclas e barra de espaco), para Scheme. */
static void DrawKeyboardIcon(float cx, float cy, Color color) {
    const float x = cx - KEYBOARD_W * 0.5f, y = cy - KEYBOARD_H * 0.5f;
    const float key = 6.0f, step = 9.0f;

    DrawRectangleRoundedLinesEx((Rectangle) {x, y, KEYBOARD_W, KEYBOARD_H}, 0.25f, 6, 4.0f, color);
    for (int row = 0; row < 2; row++) {          // duas fileiras de teclas
        const float ky = cy - 11.0f + row * 9.0f;
        for (int k = 0; k < 5; k++)
            DrawRectangleRec((Rectangle) {cx - 2.0f * step + k * step - key * 0.5f + 0.5f, ky, key, key}, color);
    }
    DrawRectangleRounded((Rectangle) {cx - 12.0f, cy + 5.0f, 24.0f, 5.0f}, 0.5f, 4, color); // espaco
}

/* Painel preto arredondado atras dos icones de notas (mesma ideia da faixa do gameplay). */
static void DrawIconPanel(float cx, float cy) {
    DrawRectangleRounded((Rectangle) {cx - ICON_PANEL_W * 0.5f, cy - ICON_PANEL_H * 0.5f, ICON_PANEL_W, ICON_PANEL_H},
                         0.4f, 8, (Color) {0, 0, 0, ICON_PANEL_ALPHA});
}

/* Teclas do Keys Monitor: 4 quadradinhos na cor de cada coluna, com a letra do esquema (como no gameplay). */
static void DrawKeysMonitorIcon(float cx, float cy) {
    static const Color laneCol[4] = {{200, 80, 255, 255}, {80, 180, 255, 255}, {80, 255, 150, 255}, {255, 90, 90, 255}};
    static const char* const laneLetter[4] = {"L", "D", "U", "R"};
    const float total = 4.0f * KEYMON_BOX + 3.0f * KEYMON_BOX_GAP;
    const float x0 = cx - total * 0.5f;
    const float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 2.0f);

    for (int i = 0; i < 4; i++) {
        const Rectangle box = {x0 + i * (KEYMON_BOX + KEYMON_BOX_GAP), cy - KEYMON_BOX * 0.5f, KEYMON_BOX, KEYMON_BOX};
        const int key = game_scheme_keys[gameOptions.scheme][i];
        char label[2] = {laneLetter[i][0], 0};
        if (key >= KEY_A && key <= KEY_Z) label[0] = (char)('A' + (key - KEY_A));
        else if (key >= KEY_ZERO && key <= KEY_NINE) label[0] = (char)('0' + (key - KEY_ZERO));

        Color lit = laneCol[i];
        lit.a = (unsigned char)(50.0f + 50.0f * pulse);   // respira devagar, como o brilho das teclas
        DrawRectangleRec(box, (Color) {0, 0, 0, 160});
        DrawRectangleRec(box, lit);
        DrawRectangleLinesEx(box, 2.0f, laneCol[i]);

        const Vector2 ts = MeasureTextEx(mainFont, label, 16, 1.0f);
        DrawTextEx(mainFont, label, (Vector2) {box.x + (box.width - ts.x) * 0.5f, box.y + (box.height - ts.y) * 0.5f},
                   16, 1.0f, WHITE);
    }
}

/* Notas pequenas (uma por coluna) que sobem devagar, crescem, somem e recomecam: "escondidas" e aparecendo. */
static void DrawHiddenNotesIcon(float cx, float cy) {
    const float t0 = (float)GetTime() / HIDDEN_ICON_PERIOD;
    for (int k = 0; k < 3; k++) {
        const float ph = t0 + (float)k / 3.0f;
        const float t = ph - floorf(ph);                 // 0..1 dentro do ciclo
        const float s = HIDDEN_ICON_SCALE * sinf(PI * t); // cresce e diminui: aparece e some
        if (s < 0.005f) continue;
        NoteColors_DrawNoteIcon(k, cx + (float)(k - 1) * 22.0f, cy + 14.0f - 28.0f * t, s);
    }
}

/* Mouse (corpo arredondado, divisao dos botoes e roda), para os Beat Zoom. */
static void DrawMouseIcon(float cx, float cy, Color color) {
    const float w = MOUSE_ICON_W, h = MOUSE_ICON_H, stroke = MOUSE_ICON_STROKE;
    const float left = cx - w * 0.5f, top = cy - h * 0.5f;

    DrawRectangleRoundedLinesEx((Rectangle) {left, top, w, h}, 0.9f, 10, stroke, color);
    DrawLineEx((Vector2) {cx, top + stroke}, (Vector2) {cx, top + h * 0.45f}, stroke * 0.7f, color);   // divisao dos botoes
    DrawRectangleRounded((Rectangle) {cx - stroke * 0.6f, top + h * 0.14f, stroke * 1.2f, h * 0.2f}, 1.0f, 6, color); // roda
}

/* Circulo com setinha (mesmo desenho do Restart da pausa), centrado em (cx, cy). */
void DrawResetIcon(float cx, float cy, Color color) {
    const float size = 36.0f, stroke = 5.0f;
    const float r = size * 0.5f - stroke * 0.5f;
    const float endDeg = 330.0f, rad = endDeg * DEG2RAD;
    const Vector2 c = {cx, cy};

    DrawRing(c, r - stroke * 0.5f, r + stroke * 0.5f, 30.0f, endDeg, 32, color);

    const Vector2 p = {c.x + r * cosf(rad), c.y + r * sinf(rad)};
    const Vector2 dir = {-sinf(rad), cosf(rad)};          // tangente no sentido do arco
    const Vector2 tip = {p.x + dir.x * 6.0f, p.y + dir.y * 6.0f};
    const Vector2 n = {-dir.y, dir.x};
    const float head = size * 0.45f;
    const Vector2 back = {tip.x - dir.x * head, tip.y - dir.y * head};
    DrawLineEx(tip, (Vector2) {back.x + n.x * head * 0.6f, back.y + n.y * head * 0.6f}, stroke, color);
    DrawLineEx(tip, (Vector2) {back.x - n.x * head * 0.6f, back.y - n.y * head * 0.6f}, stroke, color);
    DrawCircleV(tip, stroke * 0.5f, color);
}

/* Monitor (tela com base), para o V-SYNC. */
static void DrawMonitorIcon(float cx, float cy, Color color) {
    const float w = MONITOR_ICON_W, h = MONITOR_ICON_H;
    const float top = cy - h * 0.5f - 5.0f, bottom = top + h;

    DrawRectangleRoundedLinesEx((Rectangle) {cx - w * 0.5f, top, w, h}, 0.14f, 6, TV_STROKE, color);
    DrawLineEx((Vector2) {cx, bottom}, (Vector2) {cx, bottom + 7.0f}, TV_STROKE, color);                 // pescoco
    DrawLineEx((Vector2) {cx - 11.0f, bottom + 7.0f}, (Vector2) {cx + 11.0f, bottom + 7.0f}, TV_STROKE, color); // base
}

/* Arvore de tecnologia: raiz, dois ramos e quatro nos, em uma cor so. */
static void DrawTechTreeIcon(float cx, float cy, Color color) {
    const Vector2 root = {cx, cy - 14.0f};
    const Vector2 mid[2] = {{cx - 12.0f, cy + 2.0f}, {cx + 12.0f, cy + 2.0f}};
    const Vector2 leaf[4] = {{cx - 18.0f, cy + 16.0f}, {cx - 6.0f, cy + 16.0f}, {cx + 6.0f, cy + 16.0f}, {cx + 18.0f, cy + 16.0f}};

    for (int m = 0; m < 2; m++) {
        DrawRoundLine(root, mid[m], color);
        for (int k = 0; k < 2; k++) DrawRoundLine(mid[m], leaf[m * 2 + k], color);
    }
    DrawCircleV(root, TREE_NODE_R + 1.0f, color);
    for (int m = 0; m < 2; m++) DrawCircleV(mid[m], TREE_NODE_R, color);
    for (int k = 0; k < 4; k++) DrawCircleV(leaf[k], TREE_NODE_R - 1.0f, color);
}

/* Rosto triste (circulo, olhos e boca curvada para baixo), para o Losing Icon. */
static void DrawSadFaceIcon(float cx, float cy, Color color) {
    const float r = FACE_ICON_R;

    DrawRing((Vector2) {cx, cy}, r - 2.5f, r + 2.5f, 0.0f, 360.0f, 36, color);
    DrawCircleV((Vector2) {cx - 6.0f, cy - 4.0f}, 2.8f, color);
    DrawCircleV((Vector2) {cx + 6.0f, cy - 4.0f}, 2.8f, color);
    DrawRing((Vector2) {cx, cy + 5.0f}, 5.0f, 7.0f, 20.0f, 160.0f, 16, color);
}

/* Robo (cabeca com antena e olhos), para o Botplay (jogada automatica). */
static void DrawRobotIcon(float cx, float cy, Color color) {
    const float w = ROBOT_ICON_W, h = ROBOT_ICON_H;
    const float top = cy - h * 0.5f - 4.0f;   // antena + cabeca ficam centrados em cy

    DrawLineEx((Vector2) {cx, top}, (Vector2) {cx, top - 7.0f}, 3.0f, color);       // antena
    DrawCircleV((Vector2) {cx, top - 9.0f}, 3.5f, color);
    DrawRectangleRoundedLinesEx((Rectangle) {cx - w * 0.5f, top, w, h}, 0.3f, 6, 4.0f, color);
    DrawCircleV((Vector2) {cx - 7.0f, top + h * 0.42f}, 3.5f, color);                // olhos
    DrawCircleV((Vector2) {cx + 7.0f, top + h * 0.42f}, 3.5f, color);
    DrawLineEx((Vector2) {cx - 6.0f, top + h - 5.0f}, (Vector2) {cx + 6.0f, top + h - 5.0f}, 3.0f, color); // boca
}

/* Alto-falante com ondas (Hit Sound). */
static void DrawSpeakerIcon(float cx, float cy, Color color) {
    DrawRectangleRec((Rectangle) {cx - 12.0f, cy - 4.0f, 6.0f, 8.0f}, color);                    // caixa
    DrawLineEx((Vector2) {cx - 6.0f, cy - 4.0f}, (Vector2) {cx + 1.0f, cy - 10.0f}, 3.0f, color); // cone
    DrawLineEx((Vector2) {cx + 1.0f, cy - 10.0f}, (Vector2) {cx + 1.0f, cy + 10.0f}, 3.0f, color);
    DrawLineEx((Vector2) {cx + 1.0f, cy + 10.0f}, (Vector2) {cx - 6.0f, cy + 4.0f}, 3.0f, color);
    DrawRing((Vector2) {cx + 1.0f, cy}, 7.0f, 9.0f, -45.0f, 45.0f, 12, color);                    // onda
}

/* Colcheia pequena (Track da musica), na mesma forma do [Musica] mas em escala menor. */
static void DrawSmallNoteIcon(float cx, float cy, Color color) {
    const float r = 5.0f, stemW = 2.5f, flagW = 7.0f, flagH = 9.0f;
    const float headY = cy + 4.0f;                  // centro da cabeca
    const float top = cy - 8.0f;                    // topo da haste
    const float stemX = cx + r - stemW * 0.5f;

    DrawCircleV((Vector2) {cx, headY}, r, color);
    DrawRectangleRec((Rectangle) {cx + r - stemW, top, stemW, headY - top}, color);
    DrawRoundLine((Vector2) {stemX, top}, (Vector2) {stemX + flagW, top + flagH}, color);
}

/* Quatro notas (uma de cada cor) ao lado do rotulo "Notes". */
void DrawNotesLabelIcon(float x, float cy) {
    for (int k = 0; k < 4; k++)
        NoteColors_DrawNoteIcon(k, x + 15.0f * (float)k, cy, 0.1f);
}

/* Painel arredondado com uma seta dupla vertical (Scroll Style). Cor segue o estado da linha. */
static void DrawScrollIcon(float cx, float cy, Color color) {
    DrawRectangleRounded((Rectangle) {cx - 20.0f, cy - 19.0f, 40.0f, 38.0f}, 0.4f, 8, (Color) {0, 0, 0, ICON_PANEL_ALPHA});
    DrawLineEx((Vector2) {cx, cy - 9.0f}, (Vector2) {cx, cy + 9.0f}, 3.0f, color);       // haste
    DrawPoly((Vector2) {cx, cy - 12.0f}, 3, 7.0f, 0.0f, color);                          // ponta de cima
    DrawPoly((Vector2) {cx, cy + 12.0f}, 3, 7.0f, 180.0f, color);                        // ponta de baixo
}

/* Icone da linha (so para as opcoes que tem), com a mesma sombra do texto. */
void DrawExtraIcon(int i, float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f + ROW_ICON_DY;

    if (i == OPTION_SCROLL_STYLE) {
        DrawScrollIcon(EXTRA_ICON_CX, cy, fill);
        return;
    }
    // Icones de notas: sem sombra propria (a nota ja tem contorno), com painel preto quando precisa.
    if (i == OPTION_STRUM_BG) {       // painel preto + as 4 notas lado a lado, na cor de cada coluna
        DrawIconPanel(74.0f, cy);
        for (int k = 0; k < 4; k++)
            NoteColors_DrawNoteIcon(k, 26.0f + 32.0f * (float)k, cy, 0.2f);
        return;
    }
    if (i == OPTION_NOTE_COLORS) {    // 2x2: uma nota de cada cor
        DrawIconPanel(EXTRA_ICON_CX, cy);
        for (int k = 0; k < 4; k++)
            NoteColors_DrawNoteIcon(k, EXTRA_ICON_CX - 10.0f + 20.0f * (float)(k % 2), cy - 10.0f + 20.0f * (float)(k / 2), 0.13f);
        return;
    }
    if (i == OPTION_HIDDEN_NOTES) {   // notas subindo e sumindo devagar
        DrawHiddenNotesIcon(EXTRA_ICON_CX, cy);
        return;
    }
    if (i == OPTION_STRUM_MOBILE) {   // as 4 teclas do Keys Monitor
        DrawKeysMonitorIcon(EXTRA_ICON_CX, cy);
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
            case OPTION_KEYBOARD_SCHEME: DrawKeyboardIcon(cx, y, color); break;
            case OPTION_ZOOMFACTOR_UI:
            case OPTION_ZOOMFACTOR_GAME: DrawMouseIcon(cx, y, color); break;
            case OPTION_RESET_CONFIG:   DrawResetIcon(cx, y, color); break;
            case OPTION_STREAM_JSON:    DrawResetIcon(cx, y, color); break;
            case OPTION_VSYNC:          DrawMonitorIcon(TV_X + TV_W * 0.5f + shift, y, color); break;
            case OPTION_THREADS:        DrawTechTreeIcon(cx, y, color); break;
            case OPTION_LOSE_ICON:      DrawSadFaceIcon(cx, y, color); break;
            case OPTION_BOTPLAY:        DrawRobotIcon(cx, y, color); break;
            case OPTION_MENU_MUSIC:     DrawSmallNoteIcon(cx, y, color); break;
            case OPTION_HITSOUND:       DrawSpeakerIcon(cx, y, color); break;
            case OPTION_HITSOUND_BOTPLAY: DrawSpeakerIcon(cx, y, color); break;
            default: return;
        }
    }
}
