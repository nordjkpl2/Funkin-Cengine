/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStage.h - stage do PlayState (padrao do engine ou mod Psych).
 */
#pragma once
#ifndef PLAYSTATE_STAGE_H
#define PLAYSTATE_STAGE_H

#include "characters/Character.h"
#include "logic/stage/Stage.h"

/* Um sprite do .lua na ordem de desenho (addLuaSprite). */
typedef struct {
    short index;      /* em objects ou animObjects */
    char animated;
} PlayStateStageItem;

/* playAnim de onBeatHit/onStepHit/onSectionHit/onCountdownTick ja resolvido. */
typedef struct {
    StageCond cond[STAGE_COND_MAX];
    uint8_t condCount, trigger, forced;
    short object, anim;  /* animObjects[object], indice da animacao */
} PlayStateStageAction;

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

    /* Ordem de desenho do .lua (NULL = stage padrao): [0, itemBack) atras, resto na frente. */
    PlayStateStageItem* items;
    int itemBack, itemCount;

    PlayStateStageAction* actions;
    int actionCount;
} PlayStateStage;

/* Callbacks do .lua para o stage carregado (StageTrigger: contagem, batida, passo, secao). */
void PlayStateStage_Trigger(int trigger, int beat, int step, int section, int counter);

/* Carrega o stage (mod .json/.lua, "void" vazio ou o padrao) e posiciona gf, dad e bf. */
void PlayStateStage_Load(PlayStateStage* stage, const char* stageName, const char* songDir,
                         Girlfriend* gf, Character* dad, Character* bf);

/* Comeca a decodificar em fundo as imagens do stage (so uma dica; PlayStateStage_Load funciona sem). */
void PlayStateStage_Prefetch(const char* stageName, const char* songDir);

/* 1 se o stage tem hide_girlfriend (consulta o .json; permite nem carregar a GF). */
char PlayStateStage_HidesGirlfriend(const char* stageName, const char* songDir);

/* Camadas atras e na frente dos personagens. paused = nao avanca animacoes. */
void PlayStateStage_DrawBack(PlayStateStage* stage, char paused);
void PlayStateStage_DrawFront(PlayStateStage* stage, char paused);

void PlayStateStage_Unload(PlayStateStage* stage);

#endif /* PLAYSTATE_STAGE_H */
