/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateRatingPopups.h - popup de rating (sick, good, bad, shit) do PlayState.
 *
 * Tambem carrega ui.png/ui.animset, que a contagem (PlayStateIntro) compartilha.
 */
#pragma once
#ifndef PLAYSTATE_RATINGPOPUPS_H
#define PLAYSTATE_RATINGPOPUPS_H

#include "Render.h"
#include "scenes/gameplay/PlayStatePlayerInput.h"

/* Carrega ui.animset/ui.png uma vez (entre as musicas) e prepara o popup.
   Retorna o conjunto de animacoes da UI, para a contagem. */
const RayAnimationHandler* PlayStateRatingPopups_LoadUi(void);

/* Esconde o popup. Chamar em todo Create, inclusive no restart. */
void PlayStateRatingPopups_Reset(void);

/* Mostra o popup de um acerto. No botplay nao faz nada (evita custo com spam de notas). */
void PlayStateRatingPopups_Show(PlayerRating rating);

/* Anima e desenha o popup. Chamar dentro da camera do HUD. */
void PlayStateRatingPopups_Draw(float deltaTime);

#endif /* PLAYSTATE_RATINGPOPUPS_H */
