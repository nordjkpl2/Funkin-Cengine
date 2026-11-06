/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateHealth.h - vida, barra de vida e icones do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_HEALTH_H
#define PLAYSTATE_HEALTH_H

#include "scenes/AllScenes.h"
#include "scenes/Character.h"

/* Posicao e cores padrao da barra. Chamar uma vez ao criar a cena. */
/* top = 1 no downscroll: barra no topo, com o mesmo espacamento da base. */
void PlayStateHealth_Init(char top);

/* Y da barra (popup de rating usa). */
float PlayStateHealth_BarY(void);

/* Vida = 0.5 e icones sem bop. Chamar em todo Create, inclusive no restart. */
void PlayStateHealth_Reset(void);

void PlayStateHealth_SetColors(Color dad, Color bf);

/* Usa healthbar_colors dos personagens; sem cor no JSON, mantem a atual. */
void PlayStateHealth_SetCharacterColors(const Character* dad, const Character* bf);

float PlayStateHealth_Get(void);
void PlayStateHealth_Set(float value);

/* Soma delta (positivo ou negativo). O topo e limitado a 1; abaixo de 0 = morte. */
void PlayStateHealth_Add(float delta);
char PlayStateHealth_IsDead(void);

/* Pulso dos icones: Bop() no beat, Update() uma vez por frame. */
void PlayStateHealth_Bop(void);
void PlayStateHealth_Update(float deltaTime);

/* X que centraliza um objeto de largura width na barra. */
float PlayStateHealth_CenterX(float width);

/* Desenha barra e icones (dad a esquerda, bf a direita). */
void PlayStateHealth_Draw(Texture2D dadIcon, Texture2D bfIcon);

#endif /* PLAYSTATE_HEALTH_H */
