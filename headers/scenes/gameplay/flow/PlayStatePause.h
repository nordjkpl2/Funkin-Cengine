/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStatePause.h - menu de pausa do PlayState.
 *
 * Input roda no Update (PlayStatePause_Update); o Draw so desenha o menu (sem camera, em
 * coordenadas do canvas 1280x720). Restart, saida e seek sao pedidos ao PlayState pelo
 * valor de retorno. Os ultimos itens (">") abrem os paineis Playback (Song speed),
 * Notes (Scroll Type/Speed e Instakill, em PlayStateChangers) e Health Multiplier (Gain/Loss, em PlayStateHealth).
 * Sem trabalho por frame: PlayStatePause_TakeRedraw diz quando algo visivel mudou.
 */
#pragma once
#ifndef PLAYSTATE_PAUSE_H
#define PLAYSTATE_PAUSE_H

typedef enum {
    PLAYSTATE_PAUSE_NONE,
    PLAYSTATE_PAUSE_RESTART,
    PLAYSTATE_PAUSE_EXIT,
    PLAYSTATE_PAUSE_SEEK_BACK,      /* seta esquerda na opcao Skip time */
    PLAYSTATE_PAUSE_SEEK_FORWARD,   /* seta direita na opcao Skip time */
    PLAYSTATE_PAUSE_SPEED,          /* Song speed mudou (audio ja aplicado): reajustar o que depende dela */
    PLAYSTATE_PAUSE_SCROLL          /* Scroll Type/Speed mudou: redesenhar a foto da cena */
} PlayStatePauseAction;

/* Zera a pausa. Chamar em todo Create. */
void PlayStatePause_Reset(void);

/* Titulo (ultima pasta de songDir) e dificuldade ("" = normal) do cabecalho. Formata uma vez. */
void PlayStatePause_SetInfo(const char* songDir, const char* diff);

/* Abre o menu (Enter durante a musica). Pausa o audio. */
void PlayStatePause_Open(void);

char PlayStatePause_IsPaused(void);

/* Uma vez por frame com a pausa aberta: input e animacoes. */
PlayStatePauseAction PlayStatePause_Update(float deltaTime);

/* 1 se algo visivel mudou desde a ultima chamada (o PlayState pede redesenho). Zera ao ler. */
char PlayStatePause_TakeRedraw(void);

/* So desenho, fora de camera. exitToEditor muda o texto da saida; current/total (segundos) vao no Skip time. */
void PlayStatePause_Draw(char exitToEditor, float currentSecs, float totalSecs);

#endif /* PLAYSTATE_PAUSE_H */
