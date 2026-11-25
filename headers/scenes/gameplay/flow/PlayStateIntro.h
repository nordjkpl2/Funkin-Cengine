/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateIntro.h - contagem regressiva (ready, set, go) antes da musica.
 *
 * O tempo da batida (songBeat.time) continua no PlayState: ele avanca aqui durante a intro.
 */
#pragma once
#ifndef PLAYSTATE_INTRO_H
#define PLAYSTATE_INTRO_H

#include "characters/Character.h"
#include "Render.h"

/* Sons da contagem (3, 2, 1, go). Carregados uma vez, entre as musicas. */
void PlayStateIntro_LoadSounds(void);

/* Guarda o conjunto de animacoes da UI e posiciona o texto da contagem. Chamar em todo Create. */
void PlayStateIntro_LoadUi(const RayAnimationHandler* ui);

/* Prepara a contagem para um bpm. Retorna o songBeat.time inicial (negativo). */
float PlayStateIntro_Start(float bpm);

/* Restart: esconde o texto da contagem antes de recomecar. */
void PlayStateIntro_Restart(void);

/* Uma vez por frame durante a intro. gf pode ser NULL (sem GF carregada). */
void PlayStateIntro_Update(float deltaTime, Girlfriend* gf);

/* So desenho. Chamar dentro da camera do HUD, enquanto a intro roda. */
void PlayStateIntro_Draw(void);

#endif /* PLAYSTATE_INTRO_H */
