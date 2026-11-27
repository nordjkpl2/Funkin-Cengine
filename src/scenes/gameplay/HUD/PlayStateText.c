/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateText.c - textos do HUD: tempo, titulo, score, misses e BOTPLAY.
 *
 * Cada texto e rasterizado uma vez numa RenderTexture e so e refeito quando o
 * conteudo, a ancora ou o zoom da janela mudam. Desenhar e so um blit.
 * Score, misses, precisao e tempo usam contorno preto (como a Psych); titulo e BOTPLAY, sombra.
 */

#include "scenes/gameplay/HUD/PlayStateText.h"
#include "scenes/gameplay/audio/PlayStateAudio.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "GameData.h"
#include "data/config/HudConfig.h"
#include "Game.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Modo Psych/Top: "Score | Misses | Accuracy" numa linha, centralizada.
#define PSYCH_SCORE_FONT 15.0f
#define PSYCH_SCORE_OFFSET 30.0f  // Psych: distancia da barra de vida (topo da linha)
#define TOP_SCORE_Y 15.0f          // Top: linha no topo (tempo fora do topo)
#define TOP_SCORE_UNDER_TIME 44.0f // Top: logo abaixo do tempo (tempo no topo)
// Bump do score ao acertar (Psych: escala 1.075 que volta em 0.2 s).
#define SCORE_BUMP_SCALE 0.075f
#define SCORE_BUMP_TIME 0.2f

// Contorno (px do canvas): Psych usa 1.25 no score e 2 no tempo.
#define OUTLINE_SCORE 0.75f   // mais fino que o do tempo (~1-2 px a menos na tela)
#define OUTLINE_TIME 1.0f

// Modo Cengine/Left: linhas empilhadas no canto de baixo (direito ou esquerdo).
#define TEXT_RIGHT_X 1270.0f
#define TEXT_LEFT_X 10.0f
#define STACK_LAST_Y 690.0f   // ultima linha (misses, ou precisao quando ligada)
#define STACK_LINE_H 18.0f
// Centro da tela (1280): tempo, linha Psych e barra de vida no mesmo eixo.
#define TEXT_CENTER_X 640.0f
// Posicoes extras do tempo (gameOptions.timePosition). Em rightdown fica logo acima do score.
#define TIME_LOW_Y 690.0f
#define TIME_RIGHTDOWN_Y 640.0f
#define TIME_TOP_Y 15.0f

// Marca da engine (opcao Engine Mark). Posicao "Right Top": canto superior direito, na linha do titulo da musica.
// Posicao "Default": centralizada, 2px acima do tempo no downscroll e abaixo dele no scroll normal.
#define MARK_FONT 13.0f
#define MARK_RIGHTUP_X 1258.0f
#define MARK_RIGHTUP_Y 27.0f
#define MARK_GAP 2.0f
// Estilo "bar" do tempo (gameOptions.timeStyle): medidas da Psych (400x19 com borda de 3).
#define TIME_BAR_W 394.0f      // area interna (preenchimento)
#define TIME_BAR_H 13.0f
#define TIME_BAR_BORDER 3.0f
#define TIME_BAR_FONT 20.0f    // igual ao tamanho do hTime: o texto passa um pouco da barra, como na Psych

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
    float outline;               // > 0: contorno com essa espessura (canvas) no lugar da sombra
    HudAlign align;
    float anchorX, anchorY;      // referencia no canvas (x conforme align)
    float blitX, blitY, blitW, blitH; // retangulo no canvas, calculado no raster
    float offX, offY;            // deslocamento do HUD Editor (px inteiros: nao muda a fase do subpixel)
} HudText;

static HudText hTime, hTitle, hScore, hMisses, hAccuracy, hPsych, hBotplay, hMark;

// Estado "ultimo valor desenhado": evita formatar texto a cada frame.
static int lastPsychScore = -1;
static int lastPsychMisses = -1;
static int lastPsychAcc = -2;    // -1 = precisao escondida
static int lastScoreDrawn = -1;
static int lastMissesDrawn = -1;
static int lastAccDrawn = -1;
static float scoreBump;          // 1 = acabou de acertar, cai ate 0
static int lastTimeSec = -1;
static int lastTimePos = -1;
static int lastTimeStyle = -1;
static int lastTimeLift = -1;
static int lastTimeText = -1;
static float timeProgress;      // 0..1 da musica (barra do estilo bar)
static float timeBarX, timeBarY; // canto superior esquerdo da barra (coordenadas logicas)
// Layout do HUD Editor (HudConfig), lido no Init.
static float timeScale = 1.0f, scoreScale = 1.0f;
static char showTime = 1, showScoreEl = 1, showTitle = 1, showBotplay = 1;

static void HudText_Style(HudText* t, float size, Color color, Color shadow,
                          float shadowX, float shadowY, HudAlign align) {
    t->size = size;
    t->color = color;
    t->shadow = shadow;
    t->shadowX = shadowX;
    t->shadowY = shadowY;
    t->outline = 0.0f;
    t->align = align;
    t->dirty = 1;
}

// Estilo com contorno (Psych): o texto ganha borda da cor border em volta.
static void HudText_StyleOutline(HudText* t, float size, Color color, Color border, float outline, HudAlign align) {
    HudText_Style(t, size, color, border, 0.0f, 0.0f, align);
    t->outline = outline;
}

static void HudText_SetAlign(HudText* t, HudAlign align) {
    if (t->align == align) return;
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
// A rt guarda cor pre-multiplicada e o alfa como cobertura (blend separado abaixo), por isso o
// blit usa BLEND_ALPHA_PREMULTIPLY. Com BLEND_ALPHA puro o alfa saia ao quadrado (bordas apagadas).
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
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    if (t->outline > 0.0f) {
        // Contorno: 8 copias em volta (so no raster; o blit continua sendo um quad).
        const float r = t->outline * s, d = r * 0.7071f;
        static const float dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for (int k = 0; k < 8; k++) {
            const float len = k < 4 ? r : d;
            DrawTextEx(vcrFont, t->text, (Vector2) {ox + dirs[k][0] * len, oy + dirs[k][1] * len}, fsize, s, t->shadow);
        }
    } else {
        DrawTextEx(vcrFont, t->text, (Vector2) {ox + t->shadowX * s, oy + t->shadowY * s}, fsize, s, t->shadow);
    }
    DrawTextEx(vcrFont, t->text, (Vector2) {ox, oy}, fsize, s, t->color);
    EndBlendMode();
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

// Blit do ultimo raster com escala k em volta do centro (1 = normal). Precisa de BLEND_ALPHA_PREMULTIPLY ativo.
static void HudText_BlitScaled(const HudText* t, float k) {
    if (t->rt.id == 0) return;
    const float w = t->blitW * k, h = t->blitH * k;
    const float x = t->blitX - (w - t->blitW) * 0.5f + t->offX;
    const float y = t->blitY - (h - t->blitH) * 0.5f + t->offY;

    DrawTexturePro(
        t->rt.texture,
        // O raster desenha com origem no topo (ortho 0,0 = canto de cima), entao a regiao ocupada
        // fica no topo da textura (v alto). Com a folga, a origem tem que ser rtH - useH;
        // raylib soma |h| em y quando h < 0, entao {rtH-useH, -useH} cobre exatamente [rtH-useH, rtH].
        (Rectangle){0.0f, (float)(t->rtH - t->useH), (float)t->useW, -(float)t->useH},
        (Rectangle){x, y, w, h},
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
}

static void HudText_Blit(const HudText* t) {
    HudText_BlitScaled(t, 1.0f);
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
    HudText_SetAnchor(&hTitle, 22.0f, 46.0f); // logo abaixo do BOTPLAY
}

void PlayStateText_Init(const char* songDir) {
    // A fonte fica carregada entre as musicas (rasterizar o TTF a cada musica era desperdicio).
    if (vcrFont.texture.id == 0) {
        vcrFont = LoadFontEx("assets/fonts/vcr.ttf", 32, 0, 0);
        SetTextureFilter(vcrFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    // Layout do HUD Editor: escala vai no tamanho do raster; deslocamento so no blit.
    const HudConfig* hud = HudConfig_Get();
    const HudElement* eTime = &hud->el[HUD_EL_TIME];
    const HudElement* eScore = &hud->el[HUD_EL_SCORE];
    const HudElement* eTitle = &hud->el[HUD_EL_TITLE];
    const HudElement* eBot = &hud->el[HUD_EL_BOTPLAY];
    timeScale = (float)eTime->scalePct / 100.0f;
    scoreScale = (float)eScore->scalePct / 100.0f;
    showTime = eTime->visible;
    showScoreEl = eScore->visible;
    showTitle = eTitle->visible;
    showBotplay = eBot->visible;
    lastTimePos = -1; // escala pode ter mudado: refaz o layout do tempo

    HudText_StyleOutline(&hTime, 20.0f * timeScale, WHITE, BLACK, OUTLINE_TIME, HUD_ALIGN_CENTER);
    HudText_SetAnchor(&hTime, TEXT_CENTER_X, 15.0f);
    HudText_SetText(&hTime, "0:00 / 0:00");

    // Ancora e alinhamento reais vem de PlayStateText_Refresh (dependem de scorePosition).
    HudText_StyleOutline(&hScore, 16.0f * scoreScale, WHITE, BLACK, OUTLINE_SCORE, HUD_ALIGN_RIGHT);
    HudText_SetText(&hScore, "Score: 0");

    HudText_StyleOutline(&hMisses, 16.0f * scoreScale, WHITE, BLACK, OUTLINE_SCORE, HUD_ALIGN_RIGHT);
    HudText_SetText(&hMisses, "Misses: 0");

    HudText_StyleOutline(&hAccuracy, 16.0f * scoreScale, WHITE, BLACK, OUTLINE_SCORE, HUD_ALIGN_RIGHT);
    HudText_SetText(&hAccuracy, "Accuracy: 0.00%");

    HudText_StyleOutline(&hPsych, PSYCH_SCORE_FONT * scoreScale, WHITE, BLACK, OUTLINE_SCORE, HUD_ALIGN_CENTER);
    HudText_SetAnchor(&hPsych, TEXT_CENTER_X, 0.0f); // y real vem de PlayStateText_Refresh

    HudText_Style(&hTitle, 13.0f * (float)eTitle->scalePct / 100.0f, (Color){225, 225, 225, 220}, (Color){0, 0, 0, 180}, 1.0f, 1.0f, HUD_ALIGN_LEFT);
    UpdateSongDisplayName(songDir);

    HudText_Style(&hBotplay, 22.0f * (float)eBot->scalePct / 100.0f, (Color){255, 255, 255, 125}, (Color){0, 0, 0, 110}, 2.0f, 2.0f, HUD_ALIGN_LEFT);
    HudText_SetAnchor(&hBotplay, 22.0f, 20.0f);
    HudText_SetText(&hBotplay, "BOTPLAY");

    HudText_Style(&hMark, MARK_FONT, (Color){255, 255, 255, 150}, (Color){0, 0, 0, 140}, 1.0f, 1.0f, HUD_ALIGN_RIGHT);
    HudText_SetAnchor(&hMark, MARK_RIGHTUP_X, MARK_RIGHTUP_Y);
    HudText_SetText(&hMark, ENGINE_VERSION_TEXT);

    hTime.offX = (float)eTime->offsetX;
    hTime.offY = (float)eTime->offsetY;
    hScore.offX = hMisses.offX = hAccuracy.offX = hPsych.offX = (float)eScore->offsetX;
    hScore.offY = hMisses.offY = hAccuracy.offY = hPsych.offY = (float)eScore->offsetY;
    hTitle.offX = (float)eTitle->offsetX;
    hTitle.offY = (float)eTitle->offsetY;
    hBotplay.offX = (float)eBot->offsetX;
    hBotplay.offY = (float)eBot->offsetY;
}

void PlayStateText_Unload(void) {
    // Mantida: reaproveitada pela proxima musica; o SO libera ao fechar o jogo.
}

void PlayStateText_Reset(void) {
    lastScoreDrawn = -1;
    lastMissesDrawn = -1;
    lastPsychScore = -1;
    lastPsychMisses = -1;
    lastPsychAcc = -2;
    lastAccDrawn = -1;
    lastTimeSec = -1;
    lastTimePos = -1;
    lastTimeStyle = -1;
    lastTimeLift = -1;
    lastTimeText = -1;
    scoreBump = 0.0f;
}

void PlayStateText_Bump(void) {
    scoreBump = 1.0f;
}

static char OneLine(char scorePosition) {
    return scorePosition == SCORE_POS_PSYCH || scorePosition == SCORE_POS_TOP;
}

// Faixa vertical ocupada pelo tempo (topo/base em coordenadas logicas), usada para posicionar a marca.
static float timeTopY, timeBottomY;

// Aplica gameOptions.timePosition e timeStyle ao tempo (so mexe quando a opcao muda).
// lift: com a precisao no canto direito ha uma linha a mais; o tempo em rightdown sobe uma linha.
static void ApplyTimePosition(int lift) {
    if (gameOptions.timePosition == lastTimePos && gameOptions.timeStyle == lastTimeStyle && lift == lastTimeLift) return;
    lastTimePos = gameOptions.timePosition;
    lastTimeStyle = gameOptions.timeStyle;
    lastTimeLift = lift;
    HudAlign align = HUD_ALIGN_CENTER;
    float x = TEXT_CENTER_X, y = TIME_TOP_Y;
    switch (gameOptions.timePosition) {
        case TIME_POS_LOW:       y = TIME_LOW_Y; break;
        case TIME_POS_RIGHTUP:   align = HUD_ALIGN_RIGHT; x = TEXT_RIGHT_X; break;
        case TIME_POS_RIGHTDOWN: align = HUD_ALIGN_RIGHT; x = TEXT_RIGHT_X; y = TIME_RIGHTDOWN_Y - (float)lift * STACK_LINE_H; break;
        default: break;
    }
    const float ts = timeScale;
    timeTopY = y;
    timeBottomY = y + TIME_BAR_FONT * ts;
    if (gameOptions.timeStyle == TIME_STYLE_BAR) {
        timeBottomY = y + (TIME_BAR_H + 2.0f * TIME_BAR_BORDER) * ts;
        // Barra centrada em x (ou terminando em x, se alinhado a direita). O texto fica sempre
        // centrado na barra: antes, a direita, ele colava na borda e a barra parecia torta.
        timeBarX = roundf(align == HUD_ALIGN_RIGHT ? x - (TIME_BAR_BORDER + TIME_BAR_W) * ts : x - TIME_BAR_W * ts * 0.5f);
        timeBarY = y + TIME_BAR_BORDER * ts;
        align = HUD_ALIGN_CENTER;
        x = timeBarX + TIME_BAR_W * ts * 0.5f;
        y = timeBarY + (TIME_BAR_H - TIME_BAR_FONT) * ts * 0.5f;
    }
    HudText_SetAlign(&hTime, align);
    HudText_SetAnchor(&hTime, x, y);
}

// Aplica gameOptions.engineMarkPos a marca. Barato (so muda ancora), entao roda todo refresh.
static void ApplyEngineMarkPosition(void) {
    if (gameOptions.engineMarkPos == ENGINE_MARK_POS_RIGHTUP) {
        hMark.offX = hMark.offY = 0.0f; // so segue o tempo na posicao Default
        HudText_SetAlign(&hMark, HUD_ALIGN_RIGHT);
        HudText_SetAnchor(&hMark, MARK_RIGHTUP_X, MARK_RIGHTUP_Y);
        return;
    }
    const float y = SCROLL_IS_DOWN(gameOptions.scroll) ? timeTopY - MARK_FONT - MARK_GAP : timeBottomY + MARK_GAP;
    hMark.offX = hTime.offX;
    hMark.offY = hTime.offY;
    HudText_SetAlign(&hMark, HUD_ALIGN_CENTER);
    HudText_SetAnchor(&hMark, TEXT_CENTER_X, y);
}

// Atualiza o conteudo dos textos (so quando o valor muda) e rasteriza os que mudaram.
// Chamar no PlayState_Update: fora de qualquer render target e de qualquer camera.
void PlayStateText_Refresh(int score, int misses, int accuracy, float songTime, char showScore, char scorePosition) {
    char buf[64];
    const char showAcc = gameOptions.showAccuracy;

    if (scoreBump > 0.0f) scoreBump = fmaxf(0.0f, scoreBump - RayGame_DeltaTime() / SCORE_BUMP_TIME);

    // Tempo
    ApplyTimePosition(showScore && showAcc && scorePosition == SCORE_POS_CENGINE);
    ApplyEngineMarkPosition();
    {
        const float total = fmaxf(PlayStateAudio_Length(), 0.0f);
        const int currentSec = (int)(songTime < 0 ? 0 : songTime);
        if (currentSec != lastTimeSec || gameOptions.timeText != lastTimeText) {
            lastTimeSec = currentSec;
            lastTimeText = gameOptions.timeText;
            const int leftSec = (int)fmaxf(total - (songTime < 0 ? 0 : songTime), 0.0f);
            if (lastTimeText == TIME_TEXT_LEFT)
                snprintf(buf, sizeof(buf), "%d:%02d", leftSec / 60, leftSec % 60);
            else if (lastTimeText == TIME_TEXT_ELAPSED)
                snprintf(buf, sizeof(buf), "%d:%02d", currentSec / 60, currentSec % 60);
            else
                snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d",
                    currentSec / 60, currentSec % 60, (int)total / 60, (int)total % 60);
            HudText_SetText(&hTime, buf);
        }
        // Progresso por frame (so usado no estilo bar; a barra nao rasteriza nada).
        timeProgress = total > 0.0f ? fminf(fmaxf(songTime / total, 0.0f), 1.0f) : 0.0f;
    }

    // Psych/Top: uma linha centralizada (sob a barra de vida ou no topo).
    if (showScore && OneLine(scorePosition)) {
        const int acc = showAcc ? accuracy : -1;
        if (score != lastPsychScore || misses != lastPsychMisses || acc != lastPsychAcc) {
            lastPsychScore = score;
            lastPsychMisses = misses;
            lastPsychAcc = acc;
            if (acc >= 0)
                snprintf(buf, sizeof(buf), "Score: %d | Misses: %d | Accuracy: %d.%02d%%", score, misses, acc / 100, acc % 100);
            else
                snprintf(buf, sizeof(buf), "Score: %d | Misses: %d", score, misses);
            HudText_SetText(&hPsych, buf);
        }
        float y = PlayStateHealth_BarY() + PSYCH_SCORE_OFFSET;
        if (scorePosition == SCORE_POS_TOP)
            y = gameOptions.timePosition == TIME_POS_DEFAULT ? TOP_SCORE_UNDER_TIME : TOP_SCORE_Y;
        HudText_SetAnchor(&hPsych, TEXT_CENTER_X, y);
    }
    // Cengine/Left: linhas empilhadas no canto de baixo (direito ou esquerdo).
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
        if (showAcc && accuracy != lastAccDrawn) {
            lastAccDrawn = accuracy;
            snprintf(buf, sizeof(buf), "Accuracy: %d.%02d%%", accuracy / 100, accuracy % 100);
            HudText_SetText(&hAccuracy, buf);
        }
        const char left = scorePosition == SCORE_POS_LEFT;
        const HudAlign align = left ? HUD_ALIGN_LEFT : HUD_ALIGN_RIGHT;
        const float x = left ? TEXT_LEFT_X : TEXT_RIGHT_X;
        const float lineH = STACK_LINE_H * scoreScale;
        const float y = STACK_LAST_Y + STACK_LINE_H - (showAcc ? 3.0f : 2.0f) * lineH; // ultima linha termina no mesmo lugar
        HudText_SetAlign(&hScore, align);
        HudText_SetAlign(&hMisses, align);
        HudText_SetAlign(&hAccuracy, align);
        HudText_SetAnchor(&hScore, x, y);
        HudText_SetAnchor(&hMisses, x, y + lineH);
        HudText_SetAnchor(&hAccuracy, x, y + 2.0f * lineH);
    }

    HudText_Flush(&hTime);
    HudText_Flush(&hTitle);
    HudText_Flush(&hScore);
    HudText_Flush(&hMisses);
    if (gameOptions.showAccuracy) HudText_Flush(&hAccuracy);
    HudText_Flush(&hPsych);
    HudText_Flush(&hBotplay);

    if (gameOptions.engineMark) HudText_Flush(&hMark);
}

// Desenha so os blits (ja rasterizados em Refresh). Dentro da camera do HUD.
void PlayStateText_Draw(char showScore, char scorePosition) {
    showScore = showScore && showScoreEl;
    if (showTime && gameOptions.timeStyle == TIME_STYLE_BAR) {
        // Igual a Psych: moldura preta solida e preenchimento branco (o texto tem contorno e le nos dois).
        const float ts = timeScale, b = TIME_BAR_BORDER * ts, w = TIME_BAR_W * ts, h = TIME_BAR_H * ts;
        const float bx = timeBarX + hTime.offX, by = timeBarY + hTime.offY;
        DrawRectangleRec((Rectangle){bx - b, by - b, w + 2.0f * b, h + 2.0f * b}, BLACK);
        if (timeProgress > 0.0f)
            DrawRectangleRec((Rectangle){bx, by, w * timeProgress, h}, WHITE);
    }

    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);

    if (showTime) HudText_Blit(&hTime);
    if (showTitle) HudText_Blit(&hTitle);
    if (gameOptions.engineMark) HudText_Blit(&hMark);

    if (showScore && OneLine(scorePosition)) {
        HudText_BlitScaled(&hPsych, 1.0f + SCORE_BUMP_SCALE * scoreBump);
    }
    else if (showScore) {
        HudText_Blit(&hScore);
        HudText_Blit(&hMisses);
        if (gameOptions.showAccuracy) HudText_Blit(&hAccuracy);
    }

    EndBlendMode();
}

void PlayStateText_DrawBotplayMark(void) {
    if (!showBotplay) return;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    HudText_Blit(&hBotplay);
    EndBlendMode();
}
