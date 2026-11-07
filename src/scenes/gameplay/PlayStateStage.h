/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStage.h - stage do PlayState (padrao do engine ou mod Psych).
 */
#pragma once
#ifndef PLAYSTATE_STAGE_H
#define PLAYSTATE_STAGE_H

#include "logic/Character.h"

typedef struct {
    RayGraphicObject* objects;
    float defaultCameraZoom;
    char hideGirlfriend;     /* stage JSON hide_girlfriend */
    float cameraSpeed;       /* stage JSON camera_speed (1 = normal) */

    int backgroundObjects;
    int objectCount;

    /* Sprites animados do stage do mod (atlas .xml). */
    RayAnimatedObject* animObjects;
    int animBackground;
    int animCount;
} PlayStateStage;

/* Carrega o stage (mod .json/.lua, "void" vazio ou o padrao) e posiciona gf, dad e bf. */
void PlayStateStage_Load(PlayStateStage* stage, const char* stageName, const char* songDir,
                         Girlfriend* gf, Character* dad, Character* bf);

/* Camadas atras e na frente dos personagens. paused = nao avanca animacoes. */
void PlayStateStage_DrawBack(PlayStateStage* stage, char paused);
void PlayStateStage_DrawFront(PlayStateStage* stage, char paused);

void PlayStateStage_Unload(PlayStateStage* stage);

#endif /* PLAYSTATE_STAGE_H */
