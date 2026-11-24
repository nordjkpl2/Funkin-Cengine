/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStrums.h - receptores (strumlines) do oponente e do BF: posicao, animacao e desenho.
 *
 * Posicao depende de gameOptions.scroll (up, down, middle). Quem decide o acerto
 * (NoteSpawn / PlayerInput) so chama as funcoes de confirmacao.
 */
#pragma once
#ifndef PLAYSTATE_STRUMS_H
#define PLAYSTATE_STRUMS_H

#include "logic/notes/Note.h"
#include "scenes/gameplay/notes/PlayStatePlayerInput.h"

#define PLAYSTATE_LANES 4

/* Carrega os receptores e posiciona conforme o scroll. Chamar no Create, depois da skin da musica. */
void PlayStateStrums_Load(void);

/* Os 4 receptores do oponente e do BF (para NoteSpawn). */
StrumNote* PlayStateStrums_Dad(void);
StrumNote* PlayStateStrums_Bf(void);

/* Animacao de confirmacao numa coluna (acerto do oponente ou do botplay). */
void PlayStateStrums_ConfirmDad(int lane);
void PlayStateStrums_ConfirmBf(int lane);

/* Anima os receptores do BF a partir das teclas deste frame. */
void PlayStateStrums_ApplyInput(const PlayStatePlayerKeys* keys);

/* Fundo do HUD (chamar antes de tudo): faixa preta dos receptores. */
void PlayStateStrums_DrawBackdrop(void);

/* Glow do monitor de teclas (Keys Monitor): chamar todo frame com keyDown[4] (zerado no botplay). */
void PlayStateStrums_UpdateGlow(const char* keyDown);

/* Monitor de teclas no canto superior direito (so com Keys Monitor ligado). Desenhar por cima das notas. */
void PlayStateStrums_DrawKeyMonitor(void);

/* Desenha os dois lados; no botplay volta o BF ao idle quando o confirm termina. */
void PlayStateStrums_Draw(void);

#endif /* PLAYSTATE_STRUMS_H */
