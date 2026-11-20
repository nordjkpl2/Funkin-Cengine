/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePause.h - menu de pausa do PlayState.
 *
 * Input roda no Update (PlayStatePause_Update); o Draw so desenha o menu.
 * Restart e saida sao pedidos ao PlayState pelo valor de retorno.
 */
#pragma once
#ifndef PLAYSTATE_PAUSE_H
#define PLAYSTATE_PAUSE_H

typedef enum {
    PLAYSTATE_PAUSE_NONE,
    PLAYSTATE_PAUSE_RESTART,
    PLAYSTATE_PAUSE_EXIT,
    PLAYSTATE_PAUSE_SEEK_BACK,      /* seta esquerda na opcao Skip time */
    PLAYSTATE_PAUSE_SEEK_FORWARD    /* seta direita na opcao Skip time */
} PlayStatePauseAction;

/* Zera a pausa. Chamar em todo Create. */
void PlayStatePause_Reset(void);

/* Abre o menu (Enter durante a musica). Pausa o audio. */
void PlayStatePause_Open(void);

char PlayStatePause_IsPaused(void);

/* Uma vez por frame com a pausa aberta: input e animacao da selecao. */
PlayStatePauseAction PlayStatePause_Update(float deltaTime);

/* So desenho. exitToEditor muda o texto da ultima opcao; current/total (segundos) vao na opcao Skip time.
   Chamar dentro da camera do HUD. */
void PlayStatePause_Draw(char exitToEditor, float currentSecs, float totalSecs);

#endif /* PLAYSTATE_PAUSE_H */
