/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAnimations.h - personagens do PlayState (oponente, BF, GF): carregar, animar,
 * desenhar e reagir a acerto/erro/sustain.
 *
 * Quem decide o acerto (NoteSpawn / PlayerInput) chama as funcoes de evento via PlayState.
 * O idle no beat e a batida ficam em PlayStateBeat.
 */
#pragma once
#ifndef PLAYSTATE_ANIMATIONS_H
#define PLAYSTATE_ANIMATIONS_H

#include "characters/Character.h"
#include "data/song/Song.h"

/* Antes de carregar: define a pasta do mod, descobre se a GF fica escondida e pre-decodifica as texturas. */
void PlayStateAnimations_Prefetch(const Song* song, const char* songDir, const char* opponent);

/* Carrega GF, oponente e BF (e a animacao de morte do BF). Chamar depois de Prefetch. */
void PlayStateAnimations_Load(const Song* song, const char* opponent);

/* Sons de acerto e erro: carregados uma vez, entre as musicas. */
void PlayStateAnimations_LoadSounds(void);

/* Volume global (0..1) do hitsound: aplicado sobre o volume base do som. */
void PlayStateAnimations_SetHitVolume(float volume);

/* Restart: volta os personagens para o idle e reaproveita o que ja esta carregado. */
void PlayStateAnimations_Restart(void);

/* O stage pediu para esconder a GF (ela continua carregada, so nao e desenhada). */
void PlayStateAnimations_HideGirlfriend(void);

/* Libera icones e conjuntos de animacao (Destroy). Sons ficam carregados. */
void PlayStateAnimations_Unload(void);

/* Acesso aos personagens (stage, morte, camera, batida). */
Character* PlayStateAnimations_Dad(void);
Character* PlayStateAnimations_Bf(void);
/* GF sempre valida (objeto do stage); GfLoaded retorna NULL quando nao ha GF carregada. */
Girlfriend* PlayStateAnimations_Gf(void);
Girlfriend* PlayStateAnimations_GfLoaded(void);

/* Evento Change Character: troca o personagem do lado (target 0 = oponente, 1 = BF, 2 = GF) pelo
 * nome da pasta/xml em name. Mantem a posicao do slot. Vazio ou nome desconhecido cai no padrao. */
void PlayStateAnimations_ChangeCharacter(int target, const char* name);

/* Eventos de nota (lane = 0..3). */
void PlayStateAnimations_OpponentHit(int lane);
void PlayStateAnimations_OpponentHold(void);
void PlayStateAnimations_BotHit(int lane);
void PlayStateAnimations_BotHold(void);
void PlayStateAnimations_PlayerHit(int lane);
void PlayStateAnimations_PlayerHold(void);
void PlayStateAnimations_PlayerMiss(int lane);

/* Uma vez por frame, fora da pausa: avanca os frames de GF, oponente e BF. */
void PlayStateAnimations_Step(void);

/* So desenho. Chamar dentro da camera de jogo, entre o stage de tras e o da frente. */
void PlayStateAnimations_Draw(void);

#endif /* PLAYSTATE_ANIMATIONS_H */
