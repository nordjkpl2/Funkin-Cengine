/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCamera.h - cameras de jogo e HUD do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_CAMERA_H
#define PLAYSTATE_CAMERA_H

#include "characters/Character.h"

/* Estado inicial das duas cameras. Chamar em todo Create. */
void PlayStateCamera_Reset(Character* initialTarget);

/* Personagem que a camera de jogo segue. */
void PlayStateCamera_Follow(Character* target);

/* Posiciona a camera de jogo sem suavizar. */
void PlayStateCamera_SetPosition(float x, float y);
void PlayStateCamera_SetGameZoom(float zoom);

/* Pulso de zoom (somado ao zoom atual). */
void PlayStateCamera_Bump(float hudZoom, float gameZoom);

/* Pulso do beat: a cada N batidas (padrao 4), vezes os fatores das opcoes. Respeita Camera Bopping / camZooming. */
void PlayStateCamera_BeatBump(int beat, float hudFactor, float gameFactor);

/* Velocidade da camera (camera_speed do stage; 1 = normal). */
void PlayStateCamera_SetSpeed(float speed);

/* Suaviza posicao e zoom. Uma vez por frame. */
void PlayStateCamera_Update(float deltaTime, float defaultGameZoom);

/* Abre a camera de jogo / HUD. Fechar com Render_StopCamera(). */
void PlayStateCamera_BeginGame(void);
void PlayStateCamera_BeginHUD(void);

/* ---- Eventos (Psych / JS Engine). ResetEvents volta tudo abaixo ao padrao. ---- */
void PlayStateCamera_ResetEvents(void);
/* Camera Follow Pos: on = 1 prende a camera em (x, y) do mundo; on = 0 volta a seguir quem canta. */
void PlayStateCamera_ForceFocus(char on, float x, float y);
/* Ponto que a camera mira agora (para "isCameraOnForcedPos" sem posicao). */
Vector2 PlayStateCamera_Target(void);
/* Screen Shake: intensidade em fracao da tela (0.05 = 5%), duracao em segundos. hud = 1 treme so o HUD. */
void PlayStateCamera_Shake(char hud, float intensity, float seconds);
/* Zoom padrao da camera (Set Camera Zoom / defaultCamZoom). 0 = o do stage. */
void PlayStateCamera_SetZoomOverride(float zoom);
/* Zoom padrao em uso (override ou o do stage). */
float PlayStateCamera_BaseZoom(void);
/* Zoom atual da camera de jogo (com pulsos). */
float PlayStateCamera_GameZoom(void);
/* cameraSpeed por evento. 0 = o do stage. */
void PlayStateCamera_SetSpeedOverride(float speed);
/* Camera Bopping: pulso a cada everyBeats (0 = desliga) com intensidade mult (1 = normal). */
void PlayStateCamera_SetBop(int everyBeats, float mult);
/* Liga/desliga o pulso do beat sem mudar a frequencia (camZooming / Enable/Disable Camera Bop). */
void PlayStateCamera_SetBopEnabled(char on);

#endif /* PLAYSTATE_CAMERA_H */
