/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: icone desenhado de cada tipo de evento. So primitivas do raylib (sem textura).
 * Coordenadas em fracao do quadrado do icone (0..1), entao funciona em qualquer tamanho. */

#include "scenes/modding/charteditor/ui/EditorEventIcons.h"
#include "scenes/api/FontLoader.h"
#include <math.h>
#include <string.h>

typedef enum {
    EVI_NONE, EVI_FADE_IN, EVI_FADE_OUT, EVI_IMAGE, EVI_IMAGE_HIDE, EVI_TV, EVI_GLITCH, EVI_BLACKHOLE,
    EVI_GRAY, EVI_BLOOM, EVI_SPACE, EVI_PULSE, EVI_DISTORT, EVI_CHROMA, EVI_DISSOLVE, EVI_DEPTH,
    EVI_VIGNETTE, EVI_HEALTH, EVI_WAVE, EVI_OFF, EVI_PARALLAX, EVI_CAMERA, EVI_CHAR, EVI_LETTERBOX,
    EVI_RECORD, EVI_HEY, EVI_GF_SPEED, EVI_ZOOM, EVI_PLAY, EVI_TARGET, EVI_IDLE, EVI_SHAKE, EVI_SCROLL,
    EVI_GEAR, EVI_SOUND, EVI_FLASH, EVI_ZOOM_SET, EVI_BOP
} EvIcon;

/* Cor de fundo por categoria. */
#define C_SCREEN (Color){50, 165, 155, 255}
#define C_SHADER (Color){140, 95, 225, 255}
#define C_CAMERA (Color){70, 135, 225, 255}
#define C_CHAR   (Color){230, 135, 55, 255}
#define C_HUD    (Color){80, 175, 90, 255}
#define C_SOUND  (Color){220, 80, 150, 255}
#define C_PROP   (Color){120, 120, 145, 255}

static const Color FG = {255, 255, 255, 255};
static const Color DK = {28, 22, 44, 255};

static const struct {
    const char* name;
    EvIcon icon;
    Color bg;
} iconTable[] = {
    {"Fade In", EVI_FADE_IN, C_SCREEN},          {"Fade Out", EVI_FADE_OUT, C_SCREEN},
    {"Show Image", EVI_IMAGE, C_SCREEN},          {"Hide Image", EVI_IMAGE_HIDE, C_SCREEN},
    {"Letterbox", EVI_LETTERBOX, C_SCREEN},       {"Music Credit", EVI_RECORD, C_SCREEN},
    {"Credits Popup", EVI_RECORD, C_SCREEN},      {"Flash Camera", EVI_FLASH, C_SCREEN},
    {"Shader TV", EVI_TV, C_SHADER},              {"Shader Glitch", EVI_GLITCH, C_SHADER},
    {"Shader Blackness", EVI_BLACKHOLE, C_SHADER}, {"Shader Grayscale", EVI_GRAY, C_SHADER},
    {"Shader Bloom", EVI_BLOOM, C_SHADER},        {"Shader Space", EVI_SPACE, C_SHADER},
    {"Shader Pulse", EVI_PULSE, C_SHADER},        {"Shader Distortion", EVI_DISTORT, C_SHADER},
    {"Shader Chromatic", EVI_CHROMA, C_SHADER},   {"Shader Dissolve", EVI_DISSOLVE, C_SHADER},
    {"Shader Depth", EVI_DEPTH, C_SHADER},        {"Shader Vignette", EVI_VIGNETTE, C_SHADER},
    {"Shader Wave", EVI_WAVE, C_SHADER},          {"Shader Off", EVI_OFF, C_SHADER},
    {"Camera Parallax", EVI_PARALLAX, C_CAMERA},  {"Camera Move", EVI_CAMERA, C_CAMERA},
    {"Add Camera Zoom", EVI_ZOOM, C_CAMERA},      {"Set Camera Zoom", EVI_ZOOM_SET, C_CAMERA},
    {"Camera Follow Pos", EVI_TARGET, C_CAMERA},  {"Screen Shake", EVI_SHAKE, C_CAMERA},
    {"Camera Bopping", EVI_BOP, C_CAMERA},        {"Enable Camera Bop", EVI_BOP, C_CAMERA},
    {"Disable Camera Bop", EVI_BOP, C_CAMERA},
    {"Change Character", EVI_CHAR, C_CHAR},       {"Hey!", EVI_HEY, C_CHAR},
    {"Play Animation", EVI_PLAY, C_CHAR},         {"Alt Idle Animation", EVI_IDLE, C_CHAR},
    {"Set GF Speed", EVI_GF_SPEED, C_CHAR},
    {"Health Bar", EVI_HEALTH, C_HUD},            {"Change Scroll Speed", EVI_SCROLL, C_HUD},
    {"Play Sound", EVI_SOUND, C_SOUND},           {"Set Property", EVI_GEAR, C_PROP},
};

/* ---- ajudantes em coordenadas do icone (u, v em 0..1) ---- */

static Vector2 P(Rectangle r, float u, float v) {
    return (Vector2){r.x + u * r.width, r.y + v * r.height};
}

static Rectangle Sub(Rectangle r, float u0, float v0, float u1, float v1) {
    return (Rectangle){r.x + u0 * r.width, r.y + v0 * r.height, (u1 - u0) * r.width, (v1 - v0) * r.height};
}

static void Line(Rectangle r, float u0, float v0, float u1, float v1, float thick, Color c) {
    DrawLineEx(P(r, u0, v0), P(r, u1, v1), thick * r.width, c);
}

/* DrawTriangle so desenha no sentido anti-horario: acerta a ordem sozinho. */
static void Tri(Rectangle r, float au, float av, float bu, float bv, float cu, float cv, Color col) {
    const Vector2 a = P(r, au, av), b = P(r, bu, bv), c = P(r, cu, cv);
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) DrawTriangle(a, c, b, col);
    else DrawTriangle(a, b, c, col);
}

static void Ring(Rectangle r, float u, float v, float inner, float outer, float a0, float a1, Color c) {
    DrawRing(P(r, u, v), inner * r.width, outer * r.width, a0, a1, 24, c);
}

static void Dot(Rectangle r, float u, float v, float radius, Color c) {
    DrawCircleV(P(r, u, v), radius * r.width, c);
}

/* Linha ondulada de u0 a u1 em torno de v (amplitude amp, ciclos ciclos). */
static void WavyLine(Rectangle r, float u0, float u1, float v, float amp, float cycles, float thick, Color c) {
    const int n = 10;
    Vector2 prev = P(r, u0, v);
    for (int i = 1; i <= n; i++) {
        const float t = (float)i / (float)n;
        const Vector2 cur = P(r, u0 + (u1 - u0) * t, v + amp * sinf(t * cycles * 2.0f * PI));
        DrawLineEx(prev, cur, thick * r.width, c);
        prev = cur;
    }
}

/* Boneco: cabeca + meio disco de ombros, centrado em (u, v) com tamanho s. */
static void Person(Rectangle r, float u, float v, float s, Color c) {
    Dot(r, u, v - 0.17f * s, 0.13f * s, c);
    DrawCircleSector(P(r, u, v + 0.32f * s), 0.27f * s * r.width, 180.0f, 360.0f, 16, c);
}

static void Magnifier(Rectangle r) {
    Ring(r, 0.43f, 0.43f, 0.18f, 0.25f, 0.0f, 360.0f, FG);
    Line(r, 0.6f, 0.6f, 0.84f, 0.84f, 0.11f, FG);
}

static void Picture(Rectangle r) {
    DrawRectangleLinesEx(Sub(r, 0.16f, 0.2f, 0.84f, 0.8f), 0.07f * r.width, FG);
    Tri(r, 0.24f, 0.72f, 0.44f, 0.42f, 0.6f, 0.72f, FG);
    Tri(r, 0.5f, 0.72f, 0.65f, 0.52f, 0.77f, 0.72f, FG);
    Dot(r, 0.66f, 0.34f, 0.07f, FG);
}

static void Initials(Rectangle r, const char* name) {
    char ab[3] = {0, 0, 0};
    int k = 0;
    char word = 1;
    for (const char* p = name; *p && k < 2; p++) {
        const char c = *p;
        const char isLetter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (isLetter && word) ab[k++] = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
        word = !isLetter;
    }
    const float fs = r.height * 0.5f;
    const Vector2 s = MeasureTextEx(mainFont, ab, fs, 1);
    DrawTextEx(mainFont, ab, (Vector2){r.x + (r.width - s.x) * 0.5f, r.y + (r.height - s.y) * 0.5f}, fs, 1, DK);
}

static void DrawGlyph(EvIcon icon, Rectangle r, Color bg) {
    switch (icon) {
    case EVI_FADE_IN:
    case EVI_FADE_OUT: {
        const Rectangle b = Sub(r, 0.16f, 0.24f, 0.84f, 0.76f);
        const Color l = icon == EVI_FADE_IN ? BLACK : FG, rt = icon == EVI_FADE_IN ? FG : BLACK;
        DrawRectangleGradientH((int)b.x, (int)b.y, (int)b.width, (int)b.height, l, rt);
        DrawRectangleLinesEx(b, 0.05f * r.width, FG);
        break;
    }
    case EVI_IMAGE: Picture(r); break;
    case EVI_IMAGE_HIDE:
        Picture(r);
        Line(r, 0.14f, 0.86f, 0.86f, 0.14f, 0.15f, bg);
        Line(r, 0.14f, 0.86f, 0.86f, 0.14f, 0.07f, DK);
        break;
    case EVI_TV:
        DrawRectangleRounded(Sub(r, 0.14f, 0.32f, 0.86f, 0.84f), 0.25f, 6, FG);
        DrawRectangleRec(Sub(r, 0.22f, 0.4f, 0.78f, 0.76f), DK);
        Line(r, 0.5f, 0.32f, 0.34f, 0.12f, 0.06f, FG);
        Line(r, 0.5f, 0.32f, 0.66f, 0.12f, 0.06f, FG);
        Line(r, 0.26f, 0.52f, 0.74f, 0.52f, 0.04f, Fade(FG, 0.6f));
        Line(r, 0.26f, 0.64f, 0.74f, 0.64f, 0.04f, Fade(FG, 0.6f));
        break;
    case EVI_GLITCH:
        DrawRectangleRec(Sub(r, 0.14f, 0.2f, 0.68f, 0.36f), (Color){255, 70, 95, 255});
        DrawRectangleRec(Sub(r, 0.3f, 0.42f, 0.86f, 0.58f), (Color){80, 235, 255, 255});
        DrawRectangleRec(Sub(r, 0.18f, 0.64f, 0.62f, 0.8f), FG);
        break;
    case EVI_BLACKHOLE:
        Ring(r, 0.5f, 0.5f, 0.2f, 0.38f, 0.0f, 360.0f, (Color){255, 205, 130, 255});
        Ring(r, 0.5f, 0.5f, 0.3f, 0.38f, 200.0f, 340.0f, FG);
        Dot(r, 0.5f, 0.5f, 0.21f, DK);
        break;
    case EVI_GRAY:
        DrawCircleSector(P(r, 0.5f, 0.5f), 0.32f * r.width, 90.0f, 270.0f, 16, FG);
        DrawCircleSector(P(r, 0.5f, 0.5f), 0.32f * r.width, -90.0f, 90.0f, 16, (Color){95, 95, 95, 255});
        Ring(r, 0.5f, 0.5f, 0.32f, 0.37f, 0.0f, 360.0f, FG);
        break;
    case EVI_BLOOM:
        Dot(r, 0.5f, 0.5f, 0.15f, FG);
        for (int i = 0; i < 8; i++) {
            const float a = (float)i * PI / 4.0f;
            Line(r, 0.5f + 0.23f * cosf(a), 0.5f + 0.23f * sinf(a), 0.5f + 0.37f * cosf(a), 0.5f + 0.37f * sinf(a), 0.06f, FG);
        }
        break;
    case EVI_SPACE:
        Tri(r, 0.42f, 0.14f, 0.35f, 0.46f, 0.49f, 0.46f, FG);
        Tri(r, 0.42f, 0.78f, 0.35f, 0.46f, 0.49f, 0.46f, FG);
        Tri(r, 0.12f, 0.46f, 0.42f, 0.39f, 0.42f, 0.53f, FG);
        Tri(r, 0.72f, 0.46f, 0.42f, 0.39f, 0.42f, 0.53f, FG);
        Dot(r, 0.76f, 0.22f, 0.05f, FG);
        Dot(r, 0.74f, 0.78f, 0.04f, FG);
        break;
    case EVI_PULSE: {
        static const float pts[][2] = {{0.08f, 0.56f}, {0.32f, 0.56f}, {0.42f, 0.26f}, {0.56f, 0.8f}, {0.66f, 0.56f}, {0.92f, 0.56f}};
        for (int i = 0; i + 1 < 6; i++) Line(r, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], 0.08f, FG);
        break;
    }
    case EVI_DISTORT:
        Ring(r, 0.5f, 0.5f, 0.33f, 0.39f, 0.0f, 360.0f, FG);
        WavyLine(r, 0.24f, 0.76f, 0.4f, 0.07f, 1.5f, 0.06f, FG);
        WavyLine(r, 0.24f, 0.76f, 0.6f, 0.07f, 1.5f, 0.06f, FG);
        break;
    case EVI_WAVE:
        WavyLine(r, 0.12f, 0.88f, 0.3f, 0.07f, 1.5f, 0.07f, FG);
        WavyLine(r, 0.12f, 0.88f, 0.5f, 0.07f, 1.5f, 0.07f, FG);
        WavyLine(r, 0.12f, 0.88f, 0.7f, 0.07f, 1.5f, 0.07f, FG);
        break;
    case EVI_CHROMA:
        Dot(r, 0.38f, 0.42f, 0.21f, (Color){255, 60, 80, 200});
        Dot(r, 0.62f, 0.42f, 0.21f, (Color){70, 230, 110, 200});
        Dot(r, 0.5f, 0.62f, 0.21f, (Color){70, 140, 255, 200});
        break;
    case EVI_DISSOLVE:
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                const float a = 1.0f - (float)(i + j) / 6.0f;
                if (a <= 0.05f || ((i * 3 + j) % 5) == 4) continue;   /* pedacos sumindo */
                const float u = 0.16f + (float)i * 0.17f, v = 0.16f + (float)j * 0.17f;
                DrawRectangleRec(Sub(r, u, v, u + 0.14f, v + 0.14f), Fade(FG, a));
            }
        }
        break;
    case EVI_DEPTH:
        DrawRectangleRec(Sub(r, 0.42f, 0.16f, 0.86f, 0.5f), Fade(FG, 0.35f));
        DrawRectangleRec(Sub(r, 0.28f, 0.32f, 0.72f, 0.66f), Fade(FG, 0.65f));
        DrawRectangleRec(Sub(r, 0.14f, 0.48f, 0.58f, 0.82f), FG);
        break;
    case EVI_VIGNETTE: {
        DrawRectangleRec(Sub(r, 0.14f, 0.2f, 0.86f, 0.8f), DK);
        const Vector2 c = P(r, 0.5f, 0.5f);
        DrawCircleGradient((int)c.x, (int)c.y, 0.3f * r.width, FG, (Color){255, 255, 255, 0});
        break;
    }
    case EVI_HEALTH:
        Dot(r, 0.36f, 0.42f, 0.16f, FG);
        Dot(r, 0.64f, 0.42f, 0.16f, FG);
        Tri(r, 0.2f, 0.47f, 0.8f, 0.47f, 0.5f, 0.84f, FG);
        break;
    case EVI_OFF:
        Ring(r, 0.5f, 0.54f, 0.22f, 0.3f, -50.0f, 230.0f, FG);
        Line(r, 0.5f, 0.12f, 0.5f, 0.5f, 0.09f, FG);
        break;
    case EVI_PARALLAX: {
        const float t = 0.06f * r.width;
        DrawRectangleLinesEx(Sub(r, 0.38f, 0.16f, 0.84f, 0.62f), t, Fade(FG, 0.6f));
        DrawRectangleLinesEx(Sub(r, 0.16f, 0.38f, 0.62f, 0.84f), t, FG);
        Line(r, 0.16f, 0.38f, 0.38f, 0.16f, 0.05f, FG);
        Line(r, 0.62f, 0.38f, 0.84f, 0.16f, 0.05f, FG);
        Line(r, 0.62f, 0.84f, 0.84f, 0.62f, 0.05f, FG);
        break;
    }
    case EVI_CAMERA:
        DrawRectangleRec(Sub(r, 0.3f, 0.24f, 0.52f, 0.36f), FG);
        DrawRectangleRounded(Sub(r, 0.12f, 0.32f, 0.88f, 0.82f), 0.25f, 6, FG);
        Dot(r, 0.5f, 0.57f, 0.16f, DK);
        Dot(r, 0.5f, 0.57f, 0.08f, FG);
        break;
    case EVI_CHAR:
        Person(r, 0.62f, 0.4f, 0.8f, Fade(FG, 0.5f));   /* o que sai */
        Person(r, 0.4f, 0.52f, 0.9f, FG);               /* o que entra */
        break;
    case EVI_IDLE:
        Person(r, 0.4f, 0.48f, 1.0f, FG);
        Ring(r, 0.76f, 0.28f, 0.08f, 0.13f, 0.0f, 270.0f, FG);
        Tri(r, 0.76f, 0.1f, 0.76f, 0.26f, 0.88f, 0.18f, FG);
        break;
    case EVI_LETTERBOX:
        DrawRectangleRec(Sub(r, 0.12f, 0.2f, 0.88f, 0.8f), FG);
        DrawRectangleRec(Sub(r, 0.12f, 0.2f, 0.88f, 0.35f), DK);
        DrawRectangleRec(Sub(r, 0.12f, 0.65f, 0.88f, 0.8f), DK);
        break;
    case EVI_RECORD:
        Dot(r, 0.5f, 0.5f, 0.38f, DK);
        Ring(r, 0.5f, 0.5f, 0.24f, 0.26f, 0.0f, 360.0f, (Color){90, 90, 90, 255});
        Ring(r, 0.5f, 0.5f, 0.31f, 0.33f, 0.0f, 360.0f, (Color){90, 90, 90, 255});
        Dot(r, 0.5f, 0.5f, 0.14f, (Color){215, 55, 70, 255});
        Dot(r, 0.5f, 0.5f, 0.035f, DK);
        break;
    case EVI_HEY:
        DrawRectangleRounded(Sub(r, 0.12f, 0.14f, 0.88f, 0.64f), 0.4f, 6, FG);
        Tri(r, 0.26f, 0.6f, 0.46f, 0.6f, 0.22f, 0.86f, FG);
        DrawRectangleRec(Sub(r, 0.46f, 0.22f, 0.54f, 0.44f), DK);
        Dot(r, 0.5f, 0.53f, 0.05f, DK);
        break;
    case EVI_GF_SPEED:
        Ring(r, 0.5f, 0.66f, 0.24f, 0.32f, 180.0f, 360.0f, FG);
        Line(r, 0.5f, 0.66f, 0.7f, 0.42f, 0.08f, FG);
        Dot(r, 0.5f, 0.66f, 0.08f, FG);
        break;
    case EVI_ZOOM:
        Magnifier(r);
        Line(r, 0.33f, 0.43f, 0.53f, 0.43f, 0.06f, FG);
        Line(r, 0.43f, 0.33f, 0.43f, 0.53f, 0.06f, FG);
        break;
    case EVI_ZOOM_SET:
        Magnifier(r);
        Line(r, 0.34f, 0.38f, 0.52f, 0.38f, 0.055f, FG);
        Line(r, 0.34f, 0.48f, 0.52f, 0.48f, 0.055f, FG);
        break;
    case EVI_PLAY:
        Ring(r, 0.5f, 0.5f, 0.31f, 0.37f, 0.0f, 360.0f, FG);
        Tri(r, 0.42f, 0.32f, 0.42f, 0.68f, 0.7f, 0.5f, FG);
        break;
    case EVI_TARGET:
        Ring(r, 0.5f, 0.5f, 0.21f, 0.27f, 0.0f, 360.0f, FG);
        Line(r, 0.5f, 0.08f, 0.5f, 0.3f, 0.06f, FG);
        Line(r, 0.5f, 0.7f, 0.5f, 0.92f, 0.06f, FG);
        Line(r, 0.08f, 0.5f, 0.3f, 0.5f, 0.06f, FG);
        Line(r, 0.7f, 0.5f, 0.92f, 0.5f, 0.06f, FG);
        Dot(r, 0.5f, 0.5f, 0.06f, FG);
        break;
    case EVI_SHAKE:
        DrawRectangleRoundedLinesEx(Sub(r, 0.32f, 0.2f, 0.68f, 0.8f), 0.3f, 6, 0.07f * r.width, FG);
        Line(r, 0.2f, 0.34f, 0.2f, 0.66f, 0.06f, FG);
        Line(r, 0.8f, 0.34f, 0.8f, 0.66f, 0.06f, FG);
        Line(r, 0.09f, 0.42f, 0.09f, 0.58f, 0.06f, FG);
        Line(r, 0.91f, 0.42f, 0.91f, 0.58f, 0.06f, FG);
        break;
    case EVI_SCROLL:
        Tri(r, 0.56f, 0.12f, 0.3f, 0.44f, 0.82f, 0.44f, FG);
        DrawRectangleRec(Sub(r, 0.47f, 0.44f, 0.65f, 0.86f), FG);
        Line(r, 0.1f, 0.56f, 0.34f, 0.56f, 0.06f, FG);
        Line(r, 0.16f, 0.7f, 0.38f, 0.7f, 0.06f, FG);
        Line(r, 0.1f, 0.84f, 0.34f, 0.84f, 0.06f, FG);
        break;
    case EVI_GEAR: {
        const Vector2 c = P(r, 0.5f, 0.5f);
        for (int i = 0; i < 4; i++)
            DrawRectanglePro((Rectangle){c.x, c.y, 0.16f * r.width, 0.74f * r.width},
                             (Vector2){0.08f * r.width, 0.37f * r.width}, (float)i * 45.0f, FG);
        Dot(r, 0.5f, 0.5f, 0.26f, FG);
        Dot(r, 0.5f, 0.5f, 0.1f, bg);
        break;
    }
    case EVI_SOUND:
        DrawRectangleRec(Sub(r, 0.12f, 0.4f, 0.28f, 0.6f), FG);
        Tri(r, 0.28f, 0.4f, 0.48f, 0.2f, 0.48f, 0.8f, FG);
        Tri(r, 0.28f, 0.4f, 0.48f, 0.8f, 0.28f, 0.6f, FG);
        Ring(r, 0.48f, 0.5f, 0.16f, 0.21f, -45.0f, 45.0f, FG);
        Ring(r, 0.48f, 0.5f, 0.29f, 0.34f, -50.0f, 50.0f, FG);
        break;
    case EVI_FLASH:
        Tri(r, 0.6f, 0.06f, 0.24f, 0.56f, 0.52f, 0.56f, FG);
        Tri(r, 0.46f, 0.44f, 0.76f, 0.44f, 0.4f, 0.94f, FG);
        break;
    case EVI_BOP:
        Dot(r, 0.5f, 0.5f, 0.12f, FG);
        Ring(r, 0.5f, 0.5f, 0.2f, 0.25f, 0.0f, 360.0f, Fade(FG, 0.75f));
        Ring(r, 0.5f, 0.5f, 0.32f, 0.37f, 0.0f, 360.0f, Fade(FG, 0.45f));
        break;
    case EVI_NONE:
        break;
    }
}

void EdEventIcon_Draw(Rectangle r, const char* name) {
    for (size_t i = 0; i < sizeof(iconTable) / sizeof(iconTable[0]); i++) {
        if (strcmp(iconTable[i].name, name) != 0) continue;
        DrawRectangleRounded(r, 0.3f, 6, iconTable[i].bg);
        DrawGlyph(iconTable[i].icon, r, iconTable[i].bg);
        return;
    }
    /* desconhecido: cor fixa pelo nome + sigla */
    unsigned h = 2166136261u;
    for (const char* p = name; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
    DrawRectangleRounded(r, 0.3f, 6, ColorFromHSV((float)(h % 360u), 0.5f, 0.9f));
    Initials(r, name);
}
