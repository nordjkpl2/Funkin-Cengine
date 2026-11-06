/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePlayerInput.h - input do jogador: teclas, julgamento, score e misses.
 *
 * O que mexe em personagem, som ou popup fica no PlayState e entra por callbacks.
 */
#pragma once
#ifndef PLAYSTATE_PLAYERINPUT_H
#define PLAYSTATE_PLAYERINPUT_H

#include "scenes/Note.h"

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

/* Processa as teclas e acerta notas. Retorna 0 no botplay (nao ha input). */
char PlayStatePlayerInput_Update(float songTime, PlayStatePlayerKeys* keys);

/* Marca a nota como perdida (vida, misses e callback). Ignorado no botplay. */
void PlayStatePlayerInput_MissNote(Note* note);

#endif /* PLAYSTATE_PLAYERINPUT_H */
