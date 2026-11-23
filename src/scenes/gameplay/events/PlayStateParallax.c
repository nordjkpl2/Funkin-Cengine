/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateParallax.c - parallax 3D falso da camera de jogo.
 * Afastamento (zoom out) + balanco em Lissajous no target + leve rolagem.
 * A intensidade vai suavemente ate o valor pedido (sem salto).
 */

#include "scenes/gameplay/events/PlayStateParallax.h"
#include <math.h>

#define PARALLAX_ZOOM_OUT 0.22f  /* amt 1 = camera 22% mais afastada */
#define PARALLAX_EASE 3.0f       /* velocidade da transicao de intensidade (1/s) */

static float target;  /* intensidade pedida pelo evento */
static float amt;     /* intensidade atual (vai ate target) */
static float spd;     /* velocidade do balanco */
static float time;    /* tempo acumulado (s) */

void PlayStateParallax_Reset(void) {
    target = 0.0f;
    amt = 0.0f;
    spd = 1.0f;
    time = 0.0f;
}

void PlayStateParallax_Set(float newAmt, float newSpd) {
    target = newAmt < 0.0f ? 0.0f : (newAmt > 2.0f ? 2.0f : newAmt);
    spd = newSpd > 0.0f ? newSpd : 1.0f;
    if (target <= 0.0f && amt <= 0.0f) time = 0.0f;
}

void PlayStateParallax_Update(float deltaTime) {
    const float step = deltaTime * PARALLAX_EASE;
    if (amt < target) amt = (amt + step < target) ? amt + step : target;
    else if (amt > target) amt = (amt - step > target) ? amt - step : target;
    if (amt <= 0.0f && target <= 0.0f) amt = 0.0f;
    time += deltaTime * spd;
}

int PlayStateParallax_Apply(const Camera2D* base, Camera2D* out) {
    if (amt <= 0.0f) return 0;

    *out = *base;
    /* afastamento: zoom menor = camera mais longe (efeito de profundidade) */
    const float breathe = 1.0f + sinf(time * 0.6f) * 0.03f * amt;
    out->zoom *= (1.0f - PARALLAX_ZOOM_OUT * amt) * breathe;
    /* balanco lateral/vertical do alvo, maior quando afastado */
    out->target.x += sinf(time * 1.0f) * amt * 160.0f;
    out->target.y += sinf(time * 1.37f) * amt * 90.0f;
    out->rotation = sinf(time * 0.73f) * amt * 3.0f;
    return 1;
}
