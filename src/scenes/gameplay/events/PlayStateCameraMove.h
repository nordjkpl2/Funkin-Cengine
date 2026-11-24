/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCameraMove.h - evento "Camera Move": zoom e deslocamento da camera de jogo, com transicao suave.
 */
#pragma once
#ifndef PLAYSTATE_CAMERA_MOVE_H
#define PLAYSTATE_CAMERA_MOVE_H

#include "raylib.h"

/* Volta ao neutro (zoom 1, sem deslocamento). */
void PlayStateCameraMove_Reset(void);
/* Novo destino: zoomPct 100 = neutro; x/y em px do mundo. */
void PlayStateCameraMove_Set(float zoomPct, float x, float y);
/* Uma vez por frame: aproxima o valor atual do destino. */
void PlayStateCameraMove_Update(float deltaTime);
/* Multiplicador de zoom atual (1 = neutro). */
float PlayStateCameraMove_Zoom(void);
/* Deslocamento atual do alvo, em px do mundo. */
Vector2 PlayStateCameraMove_Offset(void);

#endif /* PLAYSTATE_CAMERA_MOVE_H */
