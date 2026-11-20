/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateBeat.h - relogio da musica do PlayState: contagem, passos, batida, secoes e idle.
 *
 * O tempo (songBeat.time) e a fonte de tempo da cena: negativo durante a contagem.
 */
#pragma once
#ifndef PLAYSTATE_BEAT_H
#define PLAYSTATE_BEAT_H

#include "data/Song.h"

/* Zera passos e secao, cria a batida com o instrumental, agenda a contagem e posiciona a camera.
   Chamar em todo Create e no restart, depois de carregar a musica e os personagens. */
void PlayStateBeat_Start(const Song* song);

/* Uma vez por frame, com a musica tocando: dispara passos e batidas. */
void PlayStateBeat_Update(void);

/* songBeat.time (segundos desde o inicio da musica; negativo na contagem). */
double PlayStateBeat_Time(void);

/* 1 = BF canta na secao atual (mustHit), 0 = Dad. Usado pelo Hidden Notes (Alternate). */
char PlayStateBeat_SingerIsBf(void);

/* Contagem regressiva ativa (songBeat.time < 0). */
char PlayStateBeat_InCountdown(void);

/* Avanca a contagem e anima o texto/GF da intro. Chamar so com InCountdown. */
void PlayStateBeat_Countdown(float deltaTime);

/* Volta oponente e BF ao idle quando o canto termina. Chamar fora da pausa. */
void PlayStateBeat_UpdateIdle(float deltaTime);

#endif /* PLAYSTATE_BEAT_H */
