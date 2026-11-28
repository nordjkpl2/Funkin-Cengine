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
#include "logic/notes/Note.h"

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

/* ---- Eventos Psych. target: 0 = oponente, 1 = BF, 2 = GF. ResetEvents desfaz tudo (restart/seek). ---- */
/* Play Animation / Hey!: toca a animacao (papel do JSON, ex.: "hey", ou nome do atlas) e segura holdSecs
 * (0 = ate acabar). Nota cantada interrompe. Retorna 0 se o personagem nao tem a animacao. */
char PlayStateAnimations_PlaySpecial(int target, const char* anim, float holdSecs);
/* Alt Idle Animation: idle vira "idle"+suffix (GF: danceLeft/danceRight+suffix). "" volta ao normal. */
void PlayStateAnimations_SetIdleSuffix(int target, const char* suffix);
/* Set GF Speed: GF danca a cada 'beats' batidas (1 = toda batida). */
void PlayStateAnimations_SetGfSpeed(int beats);
/* Batida: GF danca (respeita GF Speed e animacao especial). Chamado por PlayStateBeat. */
void PlayStateAnimations_GfBeat(int beat);
/* Set Property <personagem>.alpha / .visible (0..1). */
void PlayStateAnimations_SetAlpha(int target, float alpha);
void PlayStateAnimations_ResetEvents(void);

/* Eventos de nota. O tipo da nota decide quem anima (Psych): sufixo -alt, GF Sing / gfSection,
 * No Animation, Hey! e as animacoes do custom_notetypes (goodNoteHit / noteMiss). */
void PlayStateAnimations_OpponentHit(const Note* note);
void PlayStateAnimations_OpponentHold(const Note* note);
void PlayStateAnimations_BotHit(const Note* note);
void PlayStateAnimations_BotHold(const Note* note);
void PlayStateAnimations_PlayerHit(const Note* note);
void PlayStateAnimations_PlayerHold(const Note* note);
/* Miss do BF na lane 0..3. note NULL = toque fantasma; nota de dano acertada toca "hurt" se existir. */
void PlayStateAnimations_PlayerMiss(int lane, const Note* note);

/* Uma vez por frame, fora da pausa: avanca os frames de GF, oponente e BF. */
void PlayStateAnimations_Step(void);

/* So desenho. Chamar dentro da camera de jogo, entre o stage de tras e o da frente. */
void PlayStateAnimations_Draw(void);

#endif /* PLAYSTATE_ANIMATIONS_H */
