/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Transition.c - veu preto de transicao entre cenas (loading -> gameplay).
 * Sem estado global espalhado: o unico estado e o modo/tempo deste modulo, usado so pelas cenas.
 */
#include "scenes/Transition.h"
#include "Game.h"
#include <stdbool.h>

typedef enum { TR_NONE, TR_IN, TR_OUT } TransitionMode;

static TransitionMode mode = TR_NONE;
static float elapsed;   // segundos desde o inicio da transicao atual
static float duration;  // duracao da transicao atual (segundos)

void Transition_FadeIn(float secs) {
    mode = TR_IN;
    elapsed = 0.0f;
    duration = secs > 0.0f ? secs : 0.0001f;
}

void Transition_FadeOut(float secs) {
    mode = TR_OUT;
    elapsed = 0.0f;
    duration = secs > 0.0f ? secs : 0.0001f;
}

bool Transition_Done(void) {
    return mode == TR_NONE || (mode == TR_OUT && elapsed >= duration);
}

void Transition_Draw(void) {
    if (mode == TR_NONE) return;

    elapsed += RayGame_DeltaTime();
    float t = elapsed / duration;
    if (t > 1.0f) t = 1.0f;
    // Suavizado (smoothstep): comeca e termina devagar, sem corte seco.
    const float s = t * t * (3.0f - 2.0f * t);
    const float alpha = mode == TR_IN ? 1.0f - s : s;

    if (mode == TR_IN && t >= 1.0f) mode = TR_NONE;
    if (alpha <= 0.0f) return;

    DrawRectangle(0, 0, 1280, 720, Fade(BLACK, alpha));
}
