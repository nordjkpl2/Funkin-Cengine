/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateParallax.c - parallax 3D falso da camera de jogo.
 * Trajetoria de Lissajous no target + leve rolagem + respiro de zoom.
 */

#include "scenes/gameplay/events/PlayStateParallax.h"
#include <math.h>

static float amt;   /* 0 = desligado; 1 = normal; 2 = maximo */
static float spd;   /* velocidade do balanco */
static float time;  /* tempo acumulado (s) */

void PlayStateParallax_Reset(void) {
    amt = 0.0f;
    spd = 1.0f;
    time = 0.0f;
}

void PlayStateParallax_Set(float newAmt, float newSpd) {
    amt = newAmt < 0.0f ? 0.0f : (newAmt > 2.0f ? 2.0f : newAmt);
    spd = newSpd > 0.0f ? newSpd : 1.0f;
    if (amt <= 0.0f) time = 0.0f;
}

void PlayStateParallax_Update(float deltaTime) {
    time += deltaTime * spd;
}

int PlayStateParallax_Apply(const Camera2D* base, Camera2D* out) {
    if (amt <= 0.0f) return 0;

    *out = *base;
    out->target.x += sinf(time * 1.0f) * amt * 90.0f;
    out->target.y += sinf(time * 1.37f) * amt * 50.0f;
    out->rotation = sinf(time * 0.73f) * amt * 2.0f;
    out->zoom *= 1.0f + cosf(time * 0.9f) * amt * 0.05f;
    return 1;
}
