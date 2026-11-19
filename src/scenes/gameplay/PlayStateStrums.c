/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStrums.c - receptores (strumlines) do oponente e do BF.
 */

#include "scenes/AllScenes.h"
#include "logic/notes/Note.h"
#include "scenes/gameplay/PlayStateStrums.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f
#define LANE_SPACING 110.0f
#define STRUM_BG_HEIGHT 150   // faixa preta atras dos receptores (Strumline Background)
#define MOBILE_COLUMN_BASE 22.0f  // opacidade parada das colunas (Strumline Mobile Visible)
#define MOBILE_COLUMN_PEAK 80.0f  // opacidade maxima ao apertar (limite baixo por causa de epilepsia)
#define MOBILE_GLOW_RISE 6.0f     // velocidade de subida (por segundo)
#define MOBILE_GLOW_FALL 2.5f     // velocidade de descida (por segundo): fade lento

/* Mesmas cores das notas no editor de chart: roxo, azul, verde, vermelho. */
static const Color laneColors[PLAYSTATE_LANES] = {
    {200, 80, 255, 255}, {80, 180, 255, 255}, {80, 255, 150, 255}, {255, 90, 90, 255},
};

/* gameOptions.scroll: 0 = up, 1 = down, 2 = middle */
static StrumNote dadStrumline[PLAYSTATE_LANES];
static StrumNote bfStrumline[PLAYSTATE_LANES];
static Vector2 dadStrumIdleSizes[PLAYSTATE_LANES];
static Vector2 bfStrumIdleSizes[PLAYSTATE_LANES];
static float bfMiddleX = 475.0f; // middlescroll: X do 1o receptor do BF (centralizado)
static float mobileGlow[PLAYSTATE_LANES]; // 0..1, sobe suave ao apertar a coluna e desce devagar (sem pisca)

static float StrumY(void) {
    if (gameOptions.scroll == 2) return 20.0f;
    if (gameOptions.scroll == 1) return 600.0f;
    return 50.0f;
}

static float StrumX(char mustHit, int lane) {
    if (gameOptions.scroll != 2)
        return (mustHit ? 700.0f : 50.0f) + LANE_SPACING * lane;

    return mustHit ? bfMiddleX + LANE_SPACING * lane : 50.0f + 55.0f * lane;
}

StrumNote* PlayStateStrums_Dad(void) { return dadStrumline; }
StrumNote* PlayStateStrums_Bf(void) { return bfStrumline; }

void PlayStateStrums_Load(void) {
    const char middle = gameOptions.scroll == 2;

    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        StrumNote_Load(dadStrumline + i, i);
        StrumNote_Load(bfStrumline + i, i);

        dadStrumline[i].object.position.y = StrumY();
        bfStrumline[i].object.position.y = StrumY();

        // Middlescroll: oponente menor; BF na escala normal das notas.
        if (middle)
            dadStrumline[i].object.scaleX = dadStrumline[i].object.scaleY = 0.5f;
    }

    if (middle) {
        RayAnimatedObject idle = bfStrumline[0].object;
        AnimatedObject_SetAnimation(&idle, StrumNote_IdleAnimation(0));
        bfMiddleX = SCREEN_W / 2 - (LANE_SPACING * (PLAYSTATE_LANES - 1) + AnimatedObject_Sizes(&idle).x) * 0.5f;
    }

    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        dadStrumline[i].object.position.x = StrumX(0, i);
        bfStrumline[i].object.position.x = StrumX(1, i);
    }

    // Cacheia o tamanho idle uma vez: nao mede/refaz a animacao a cada frame.
    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        RayAnimatedObject dadIdle = dadStrumline[i].object;
        RayAnimatedObject bfIdle = bfStrumline[i].object;

        AnimatedObject_SetAnimation(&dadIdle, StrumNote_IdleAnimation(i));
        AnimatedObject_SetAnimation(&bfIdle, StrumNote_IdleAnimation(i));

        dadStrumIdleSizes[i] = AnimatedObject_Sizes(&dadIdle);
        bfStrumIdleSizes[i] = AnimatedObject_Sizes(&bfIdle);
    }
}

/*
 * Desenha o receptor mantendo o centro visual da animacao idle como ancora.
 * Animacoes maiores (confirm, por causa do brilho) fariam a seta "saltar";
 * o tamanho idle e medido uma vez no load. A posicao logica da coluna e
 * restaurada depois do desenho, entao notas e sustains usam a coordenada fixa.
 */
static void RenderStrumFixedCenter(StrumNote* strum, Vector2 idleSize) {
    Vector2 currentSize = AnimatedObject_Sizes(&strum->object);
    Vector2 originalPosition = strum->object.position;

    strum->object.position.x += (idleSize.x - currentSize.x) * 0.5f;
    strum->object.position.y += (idleSize.y - currentSize.y) * 0.5f;

    Render_DrawAnimatedObject(&strum->object);

    strum->object.position = originalPosition;
}

/* Fundos atras de tudo do HUD: colunas coloridas (mobile) e faixa preta dos receptores. */
void PlayStateStrums_DrawBackdrop(void) {
    if (gameOptions.strumMobileVisible) {
        const float colW = SCREEN_W / PLAYSTATE_LANES;
        for (int i = 0; i < PLAYSTATE_LANES; i++) {
            // Base bem fraca; acende so a coluna pressionada, com alpha limitado (sem flash forte).
            const float a = MOBILE_COLUMN_BASE + (MOBILE_COLUMN_PEAK - MOBILE_COLUMN_BASE) * mobileGlow[i];
            Color c = laneColors[i];
            c.a = (unsigned char)a;
            DrawRectangleRec((Rectangle) {colW * (float)i, 0.0f, colW, SCREEN_H}, c);
        }
    }

    if (gameOptions.strumBgPercent > 0) {
        const unsigned char alpha = (unsigned char)(gameOptions.strumBgPercent * 255 / 100);
        DrawRectangle(0, (int)StrumY() - 10, (int)SCREEN_W, STRUM_BG_HEIGHT, (Color) {0, 0, 0, alpha});
    }
}

void PlayStateStrums_Draw(void) {
    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        AnimatedObject_UpdateFrame(&dadStrumline[i].object);
        RenderStrumFixedCenter(dadStrumline + i, dadStrumIdleSizes[i]);

        AnimatedObject_UpdateFrame(&bfStrumline[i].object);
        RenderStrumFixedCenter(bfStrumline + i, bfStrumIdleSizes[i]);
    }

    // Botplay: volta o BF ao idle quando a animacao de confirm termina.
    if (gameOptions.botplay) {
        for (int i = 0; i < PLAYSTATE_LANES; i++) {
            if (AnimatedObject_FinishedAnimation(&bfStrumline[i].object))
                AnimatedObject_SetAnimation(&bfStrumline[i].object, StrumNote_IdleAnimation(i));
        }
    }
}

void PlayStateStrums_ConfirmDad(int lane) {
    AnimatedObject_SetAnimation(&dadStrumline[lane].object, StrumNote_ConfirmAnimation(lane));
}

void PlayStateStrums_ConfirmBf(int lane) {
    AnimatedObject_SetAnimation(&bfStrumline[lane].object, StrumNote_ConfirmAnimation(lane));
}

// Animacao dos receptores do BF a partir das teclas deste frame.
void PlayStateStrums_ApplyInput(const PlayStatePlayerKeys* keys) {
    const float dt = GetFrameTime();
    for (int i = 0; i < PLAYSTATE_LANES; i++) {
        // Glow das colunas: sobe rapido quando a tecla esta segurada, desce devagar.
        const float target = keys->keyDown[i] ? 1.0f : 0.0f;
        const float speed = target > mobileGlow[i] ? MOBILE_GLOW_RISE : MOBILE_GLOW_FALL;
        const float step = speed * dt > 1.0f ? 1.0f : speed * dt;
        mobileGlow[i] += (target - mobileGlow[i]) * step;

        RayAnimatedObject* strum = &bfStrumline[i].object;

        if (keys->pressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_ConfirmAnimation(i));
        else if (!keys->keyDown[i]) {
            const int idle = StrumNote_IdleAnimation(i);
            if (strum->currentAnim.animationIndex != idle)
                AnimatedObject_SetAnimation(strum, idle);
        }
        else if (keys->keyPressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_PressAnimation(i));
    }
}
