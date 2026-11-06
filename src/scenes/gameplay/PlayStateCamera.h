/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCamera.h - cameras de jogo e HUD do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_CAMERA_H
#define PLAYSTATE_CAMERA_H

#include "scenes/Character.h"

/* Estado inicial das duas cameras. Chamar em todo Create. */
void PlayStateCamera_Reset(Character* initialTarget);

/* Personagem que a camera de jogo segue. */
void PlayStateCamera_Follow(Character* target);

/* Posiciona a camera de jogo sem suavizar. */
void PlayStateCamera_SetPosition(float x, float y);
void PlayStateCamera_SetGameZoom(float zoom);

/* Pulso de zoom no beat (somado ao zoom atual). */
void PlayStateCamera_Bump(float hudZoom, float gameZoom);

/* Velocidade da camera (camera_speed do stage; 1 = normal). */
void PlayStateCamera_SetSpeed(float speed);

/* Suaviza posicao e zoom. Uma vez por frame. */
void PlayStateCamera_Update(float deltaTime, float defaultGameZoom);

/* Abre a camera de jogo / HUD. Fechar com Render_StopCamera(). */
void PlayStateCamera_BeginGame(void);
void PlayStateCamera_BeginHUD(void);

#endif /* PLAYSTATE_CAMERA_H */
