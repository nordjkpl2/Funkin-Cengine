/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * AudioStretch.h - velocidade da musica sem mudar o tom (WSOLA + sinc).
 *
 * O stream toca com pitch = speed (o raylib reamostra, e o relogio da musica continua certo).
 * Um processor do stream devolve o tom ao original (reamostra + WSOLA). Em 1.0x nada e processado.
 * Slots fixos: um por faixa (inst + vozes). Chamar so da thread principal.
 */
#pragma once
#ifndef AUDIO_STRETCH_H
#define AUDIO_STRETCH_H

#include "raylib.h"

#define AUDIOSTRETCH_SLOTS 4

/* Liga no stream do slot (recria os buffers: usar tambem depois de seek). speed 1 = so desliga. */
void AudioStretch_Set(int slot, Music* music, float speed);

/* Desliga e volta o pitch a 1. Chamar antes de UnloadMusicStream. */
void AudioStretch_Off(int slot, Music* music);

/* Atraso da saida do slot, em segundos de musica (0 quando desligado). */
double AudioStretch_Delay(int slot);

#endif /* AUDIO_STRETCH_H */
