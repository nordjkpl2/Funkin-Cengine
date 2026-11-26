/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePlayerInput.h - input do jogador: teclas, julgamento, score e misses.
 *
 * O que mexe em personagem, som ou popup fica no PlayState e entra por callbacks.
 */
#pragma once
#ifndef PLAYSTATE_PLAYERINPUT_H
#define PLAYSTATE_PLAYERINPUT_H

#include "logic/notes/Note.h"

typedef enum {
    PLAYER_RATING_SHIT,
    PLAYER_RATING_BAD,
    PLAYER_RATING_GOOD,
    PLAYER_RATING_SICK
} PlayerRating;

typedef struct {
    void (*hit)(Note* note, PlayerRating rating); /* nota acertada */
    void (*miss)(int lane);                       /* nota perdida ou toque fantasma */
} PlayStatePlayerInputEvents;

/* Estado das 4 teclas neste frame (para animar os receptores). */
typedef struct {
    char pressed[4];    /* acertou uma nota */
    int keyPressed[4];  /* tecla apertada neste frame */
    char keyDown[4];    /* tecla segurada */
} PlayStatePlayerKeys;

void PlayStatePlayerInput_SetEvents(const PlayStatePlayerInputEvents* events);

/* Zera score e misses. */
void PlayStatePlayerInput_Reset(void);

int PlayStatePlayerInput_Score(void);
int PlayStatePlayerInput_Misses(void);
void PlayStatePlayerInput_AddScore(int amount);
/* Conta um acerto na precisao (o botplay chama com SICK). */
void PlayStatePlayerInput_CountHit(PlayerRating rating);
/* Precisao em centesimos de % (0..10000), estilo Psych: acerto no tempo sobe, fora do tempo desce, miss desce. */
int PlayStatePlayerInput_Accuracy(void);

/* Co-Op e Play as: quem controla a nota. 0 = jogador 1, 1 = jogador 2, -1 = automatico (oponente, BF da IA, botplay). */
int PlayStatePlayerInput_Owner(const Note* note);
/* Jogadores humanos na partida: 1, ou 2 no Co-Op. */
int PlayStatePlayerInput_Slots(void);
/* Tecla do jogador (slot 0 ou 1) na coluna lane. */
int PlayStatePlayerInput_Key(int slot, int lane);
/* 1 se o slot toca no lado do BF; 0 se toca no lado do oponente. */
char PlayStatePlayerInput_SlotSideBf(int slot);

/* Processa as teclas de cada jogador (keys[0] e keys[1]) e acerta notas. Retorna 0 no botplay (nao ha input). */
char PlayStatePlayerInput_Update(float songTime, PlayStatePlayerKeys keys[2]);

/* Marca a nota como perdida (vida, misses e callback). Ignorado no botplay. */
void PlayStatePlayerInput_MissNote(Note* note);

#endif /* PLAYSTATE_PLAYERINPUT_H */
