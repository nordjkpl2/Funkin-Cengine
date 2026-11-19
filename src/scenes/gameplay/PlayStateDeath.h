/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateDeath.h - tela de morte do PlayState: animacao, musicas e retry.
 *
 * Restart e saida sao pedidos ao PlayState pelo valor de retorno.
 */
#pragma once
#ifndef PLAYSTATE_DEATH_H
#define PLAYSTATE_DEATH_H

#include "characters/Character.h"

typedef enum {
    PLAYSTATE_DEATH_NONE,
    PLAYSTATE_DEATH_RESTART, /* retry concluido: reiniciar a cena */
    PLAYSTATE_DEATH_LEAVE    /* ENTER, BACKSPACE ou ESC: sair da cena */
} PlayStateDeathAction;

/* Anima o BF na morte. Chamar uma vez no Create (depois de carregar o BF). */
void PlayStateDeath_LoadAnimations(Character* bf);

/* Sons e musicas de morte: carregados uma vez, entre as musicas. */
void PlayStateDeath_LoadSounds(void);

/* Volume global das musicas de morte. */
void PlayStateDeath_SetVolume(float volume);

/* Estado inicial. Chamar em todo Create, inclusive no restart. */
void PlayStateDeath_Reset(void);

/* Para as musicas e o som de morte (Destroy). */
void PlayStateDeath_Stop(void);

/* Uma vez por frame enquanto o BF estiver morto. */
PlayStateDeathAction PlayStateDeath_Update(float deltaTime, Character* bf);

#endif /* PLAYSTATE_DEATH_H */
