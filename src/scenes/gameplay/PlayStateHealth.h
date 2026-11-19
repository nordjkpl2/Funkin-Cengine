/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateHealth.h - vida, barra de vida e icones do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_HEALTH_H
#define PLAYSTATE_HEALTH_H

#include "characters/Character.h"

/* Variacao de vida por evento (0-1). Fonte unica para PlayState e PlayStatePlayerInput. */
#define HEALTH_GAIN_HIT   0.0115f  /* acerto de nota (jogador ou botplay) */
#define HEALTH_DRAIN_OPP  0.006f   /* perda por nota do oponente (drain ligado) */
#define HEALTH_LOSS_MISS  0.02f    /* miss de nota ou ghost tap */

/* Posicao e cores padrao da barra. Chamar uma vez ao criar a cena. */
/* top = 1 no downscroll: barra no topo, com o mesmo espacamento da base. */
/* mortal = 0: a vida nunca fica abaixo de 0 (imortal, ex.: botplay sem drain ou com drain). */
void PlayStateHealth_Init(char top, char mortal);

/* Y da barra (popup de rating usa). */
float PlayStateHealth_BarY(void);

/* Vida = 0.5 e icones sem bop. Chamar em todo Create, inclusive no restart. */
void PlayStateHealth_Reset(void);


/* Usa healthbar_colors dos personagens; sem cor no JSON, mantem a atual. */
void PlayStateHealth_SetCharacterColors(const Character* dad, const Character* bf);

/* Soma delta (positivo ou negativo). O topo e limitado a 1; abaixo de 0 = morte (so se mortal). */
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
