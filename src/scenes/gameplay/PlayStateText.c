/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateText.c - textos do HUD: tempo, titulo, score, misses e BOTPLAY.
 *
 * Cada texto e rasterizado uma vez numa RenderTexture e so e refeito quando o
 * conteudo, a ancora ou o zoom da janela mudam. Desenhar e so um blit.
 */

#include "scenes/gameplay/PlayStateText.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "scenes/gameplay/PlayStateHealth.h"
#include "GameData.h"
#include "Game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Modo Psych: "Score | Misses" centralizado logo abaixo da barra de vida.
#define PSYCH_SCORE_FONT 16.0f
#define PSYCH_SCORE_OFFSET 30.0f // distancia da barra de vida (topo da linha)

// Canto inferior direito (titulo, score e misses alinhados a direita).
#define TEXT_RIGHT_X 1270.0f
// Tempo do topo, centralizado igual a barra de vida.
#define TEXT_CENTER_X 600.0f

// Margem do raster (px) para a sombra nao ser cortada.
#define HUD_TEXT_PAD 4.0f
// Folga extra na RenderTexture: a textura so cresce, para o score/misses nao recriar a GPU a cada digito.
#define HUD_TEXT_SLACK 64

static Font vcrFont;

typedef enum { HUD_ALIGN_LEFT, HUD_ALIGN_CENTER, HUD_ALIGN_RIGHT } HudAlign;

// Um texto do HUD: rasterizado em rt; blit no canvas (coordenadas logicas).
typedef struct {
    RenderTexture2D rt;          // id 0 = ainda nao rasterizado
    int rtW, rtH;                // capacidade da rt em px (so cresce)
    int useW, useH;              // regiao da rt ocupada pelo ultimo raster
    char text[64];
    char dirty;                  // precisa rasterizar de novo
    float scale;                 // zoom da janela usado no ultimo raster
    float size;
    Color color, shadow;
    float shadowX, shadowY;      // deslocamento da sombra (canvas)
    HudAlign align;
    float anchorX, anchorY;      // referencia no canvas (x conforme align)
    float blitX, blitY, blitW, blitH; // retangulo no canvas, calculado no raster
} HudText;

static HudText hTime, hTitle, hScore, hMisses, hPsych, hBotplay;

// Estado "ultimo valor desenhado": evita formatar texto a cada frame.
static int lastPsychScore = -1;
static int lastPsychMisses = -1;
static int lastScoreDrawn = -1;
static int lastMissesDrawn = -1;
static int lastTimeSec = -1;

static void HudText_Style(HudText* t, float size, Color color, Color shadow,
                          float shadowX, float shadowY, HudAlign align) {
    t->size = size;
    t->color = color;
    t->shadow = shadow;
    t->shadowX = shadowX;
    t->shadowY = shadowY;
    t->align = align;
    t->dirty = 1;
}

static void HudText_SetText(HudText* t, const char* s) {
    if (strcmp(t->text, s) == 0) return;
    snprintf(t->text, sizeof(t->text), "%s", s);
    t->dirty = 1;
}

static void HudText_SetAnchor(HudText* t, float x, float y) {
    if (x == t->anchorX && y == t->anchorY) return;
    t->anchorX = x;
    t->anchorY = y;
    t->dirty = 1;
}

// Desenha o texto (sombra + cor) numa RenderTexture no zoom s.
// REGRA: so pode rodar fora de qualquer render target (BeginTextureMode nao aninha:
// o EndTextureMode desliga o framebuffer da cena e o HUD some/pisca). Por isso roda no Update.
// A rt guarda cor pre-multiplicada (BLEND_ALPHA), por isso o blit usa BLEND_ALPHA_PREMULTIPLY.
static void HudText_Raster(HudText* t, float s) {
    const float fsize = t->size * s;
    const Vector2 m = MeasureTextEx(vcrFont, t->text, fsize, s);
    const float logicalW = m.x / s;

    float lx = t->anchorX;
    if (t->align == HUD_ALIGN_CENTER)     lx -= logicalW * 0.5f;
    else if (t->align == HUD_ALIGN_RIGHT) lx -= logicalW;
    const float ly = t->anchorY;

    // Parte fracionaria da posicao vai para dentro do raster: mantem a fase do subpixel.
    const float bx = floorf(lx);
    const float by = floorf(ly);
    const float pad = ceilf(HUD_TEXT_PAD * s);
    const float textH = fmaxf(m.y, fsize);

    const int w = (int)ceilf(m.x + 2.0f * pad + s) + 2;
    const int h = (int)ceilf(textH + 2.0f * pad + s) + 2;

    if (t->rt.id == 0 || w > t->rtW || h > t->rtH) {
        if (t->rt.id != 0) UnloadRenderTexture(t->rt);
        t->rtW = w + HUD_TEXT_SLACK;
        t->rtH = h + HUD_TEXT_SLACK;
        t->rt = LoadRenderTexture(t->rtW, t->rtH);
        SetTextureFilter(t->rt.texture, TEXTURE_FILTER_BILINEAR);
    }

    const float ox = pad + (lx - bx) * s;
    const float oy = pad + (ly - by) * s;

    BeginTextureMode(t->rt);
    ClearBackground(BLANK);
    DrawTextEx(vcrFont, t->text, (Vector2) {ox + t->shadowX * s, oy + t->shadowY * s}, fsize, s, t->shadow);
    DrawTextEx(vcrFont, t->text, (Vector2) {ox, oy}, fsize, s, t->color);
    EndTextureMode();

    t->blitX = bx - pad / s;
    t->blitY = by - pad / s;
    t->blitW = (float)w / s;
    t->blitH = (float)h / s;
    t->useW = w;
    t->useH = h;
    t->scale = s;
    t->dirty = 0;
}

// Rasteriza so se mudou algo (conteudo, ancora ou zoom). Texto vazio nao gera nada.
static void HudText_Flush(HudText* t) {
    if (t->text[0] == '\0') return;

    float s = RayGame_ZoomFactor();
    if (s <= 0.0f) s = 1.0f;

    if (t->dirty || t->rt.id == 0 || t->scale != s)
        HudText_Raster(t, s);
}

// Blit do ultimo raster. Precisa de BLEND_ALPHA_PREMULTIPLY ativo (ver Draw).
static void HudText_Blit(const HudText* t) {
    if (t->rt.id == 0) return;

    DrawTexturePro(
        t->rt.texture,
        // O raster desenha com origem no topo (ortho 0,0 = canto de cima), entao a regiao ocupada
        // fica no topo da textura (v alto). Com a folga, a origem tem que ser rtH - useH;
        // raylib soma |h| em y quando h < 0, entao {rtH-useH, -useH} cobre exatamente [rtH-useH, rtH].
        (Rectangle){0.0f, (float)(t->rtH - t->useH), (float)t->useW, -(float)t->useH},
        (Rectangle){t->blitX, t->blitY, t->blitW, t->blitH},
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

// Extrai o nome da pasta da musica e prepara um titulo curto para o HUD.
static void UpdateSongDisplayName(const char* songDir) {
    const char* end = songDir + strlen(songDir);

    while (end > songDir && (end[-1] == '/' || end[-1] == '\\'))
        end--;

    const char* start = end;
    while (start > songDir && start[-1] != '/' && start[-1] != '\\')
        start--;

    char name[128];
    size_t length = (size_t)(end - start);
    if (length == 0) {
        snprintf(name, sizeof(name), "Unknown Song");
    } else {
        if (length >= sizeof(name))
            length = sizeof(name) - 1;

        memcpy(name, start, length);
        name[length] = '\0';
    }

    // Deixa nomes de pastas como "song-name" mais legiveis no HUD.
    for (char* c = name; *c; c++) {
        if (*c == '_' || *c == '-')
            *c = ' ';
    }

    Vector2 textSize = MeasureTextEx(vcrFont, name, 13.0f, 1.0f);
    while (textSize.x > 350.0f && strlen(name) > 4) {
        length = strlen(name) - 1;
        name[length] = '\0';

        if (length >= 3) {
            name[length - 3] = '.';
            name[length - 2] = '.';
            name[length - 1] = '.';
        }

        textSize = MeasureTextEx(vcrFont, name, 13.0f, 1.0f);
    }

    HudText_SetText(&hTitle, name);
    HudText_SetAnchor(&hTitle, TEXT_RIGHT_X, 651.0f);
}

void PlayStateText_Init(const char* songDir) {
    // A fonte fica carregada entre as musicas (rasterizar o TTF a cada musica era desperdicio).
    if (vcrFont.texture.id == 0) {
        vcrFont = LoadFontEx("assets/fonts/vcr.ttf", 32, 0, 0);
        SetTextureFilter(vcrFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    HudText_Style(&hTime, 20.0f, WHITE, BLACK, 1.5f, 0.0f, HUD_ALIGN_CENTER);
    HudText_SetAnchor(&hTime, TEXT_CENTER_X, 15.0f);
    HudText_SetText(&hTime, "0:00 / 0:00");

    HudText_Style(&hScore, 16.0f, WHITE, BLACK, 1.5f, -1.5f, HUD_ALIGN_RIGHT);
    HudText_SetAnchor(&hScore, TEXT_RIGHT_X, 672.0f);
    HudText_SetText(&hScore, "Score: 0");

    HudText_Style(&hMisses, 16.0f, WHITE, BLACK, 1.5f, -1.5f, HUD_ALIGN_RIGHT);
    HudText_SetAnchor(&hMisses, TEXT_RIGHT_X, 690.0f);
    HudText_SetText(&hMisses, "Misses: 0");

    HudText_Style(&hPsych, PSYCH_SCORE_FONT, WHITE, BLACK, 1.5f, -1.5f, HUD_ALIGN_CENTER);
    HudText_SetAnchor(&hPsych, TEXT_CENTER_X, 0.0f); // y real vem de PlayStateText_Refresh

    HudText_Style(&hTitle, 13.0f, (Color){225, 225, 225, 220}, (Color){0, 0, 0, 180}, 1.0f, 1.0f, HUD_ALIGN_RIGHT);
    UpdateSongDisplayName(songDir);

    HudText_Style(&hBotplay, 22.0f, (Color){255, 255, 255, 125}, (Color){0, 0, 0, 110}, 2.0f, 2.0f, HUD_ALIGN_LEFT);
    HudText_SetAnchor(&hBotplay, 22.0f, 20.0f);
    HudText_SetText(&hBotplay, "BOTPLAY");
}

void PlayStateText_Unload(void) {
    // Mantida: reaproveitada pela proxima musica; o SO libera ao fechar o jogo.
}

void PlayStateText_Reset(void) {
    lastScoreDrawn = -1;
    lastMissesDrawn = -1;
    lastPsychScore = -1;
    lastPsychMisses = -1;
    lastTimeSec = -1;
}

// Atualiza o conteudo dos textos (so quando o valor muda) e rasteriza os que mudaram.
// Chamar no PlayState_Update: fora de qualquer render target e de qualquer camera.
void PlayStateText_Refresh(int score, int misses, float songTime, char showScore, char scorePosition) {
    char buf[64];

    // Tempo
    {
        int currentSec = (int)(songTime < 0 ? 0 : songTime);
        if (currentSec != lastTimeSec) {
            lastTimeSec = currentSec;
            float total = PlayStateAudio_Length();
            if (total < 0) total = 0;
            snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d",
                currentSec / 60, currentSec % 60, (int)total / 60, (int)total % 60);
            HudText_SetText(&hTime, buf);
        }
    }

    // Score e Misses (Cengine: canto direito; Psych: centro sob a barra)
    if (showScore && scorePosition == SCORE_POS_PSYCH) {
        if (score != lastPsychScore || misses != lastPsychMisses) {
            lastPsychScore = score;
            lastPsychMisses = misses;
            snprintf(buf, sizeof(buf), "Score: %d | Misses: %d", score, misses);
            HudText_SetText(&hPsych, buf);
        }
        HudText_SetAnchor(&hPsych, TEXT_CENTER_X, PlayStateHealth_BarY() + PSYCH_SCORE_OFFSET);
    }
    else if (showScore) {
        if (score != lastScoreDrawn) {
            lastScoreDrawn = score;
            snprintf(buf, sizeof(buf), "Score: %d", score);
            HudText_SetText(&hScore, buf);
        }
        if (misses != lastMissesDrawn) {
            lastMissesDrawn = misses;
            snprintf(buf, sizeof(buf), "Misses: %d", misses);
            HudText_SetText(&hMisses, buf);
        }
    }

    HudText_Flush(&hTime);
    HudText_Flush(&hTitle);
    HudText_Flush(&hScore);
    HudText_Flush(&hMisses);
    HudText_Flush(&hPsych);
    HudText_Flush(&hBotplay);
}

// Desenha so os blits (ja rasterizados em Refresh). Dentro da camera do HUD.
void PlayStateText_Draw(char showScore, char scorePosition) {
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);

    HudText_Blit(&hTime);
    HudText_Blit(&hTitle);

    if (showScore && scorePosition == SCORE_POS_PSYCH) {
        HudText_Blit(&hPsych);
    }
    else if (showScore) {
        HudText_Blit(&hScore);
        HudText_Blit(&hMisses);
    }

    EndBlendMode();
}

void PlayStateText_DrawBotplayMark(void) {
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    HudText_Blit(&hBotplay);
    EndBlendMode();
}
