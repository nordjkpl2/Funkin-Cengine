/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateParallax.h - parallax 3D falso da camera de jogo (evento "Camera Parallax").
 */
#pragma once
#ifndef PLAYSTATE_PARALLAX_H
#define PLAYSTATE_PARALLAX_H

#include "raylib.h"

/* Zera o efeito. Chamar em Create e em Reset/Seek. */
void PlayStateParallax_Reset(void);

/* amt: 0 desliga, 1 = normal (0..2). spd: velocidade do balanco (1 = normal). */
void PlayStateParallax_Set(float amt, float spd);

/* Avanca o tempo do balanco. Uma vez por frame. */
void PlayStateParallax_Update(float deltaTime);

/* Se ativo, grava em 'out' a camera com o balanco aplicado a partir de 'base' e retorna 1.
 * Se inativo, retorna 0 e 'out' nao e tocado. */
int PlayStateParallax_Apply(const Camera2D* base, Camera2D* out);

#endif /* PLAYSTATE_PARALLAX_H */
