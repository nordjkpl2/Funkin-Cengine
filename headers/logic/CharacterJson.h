/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterJson.h - Declaracoes do JSON de personagem (Psych).
 */
#pragma once
#ifndef CHARACTER_JSON_H
#define CHARACTER_JSON_H

#include <stdbool.h>
#include "logic/CharacterInternal.h"

/* Papel de uma animacao no jogo ("idle", "singLEFT"...) ligado ao nome dela no atlas. */
typedef struct {
    char role[16];
    char sprite[64];
    int fps;
    bool looped;
    float offsetX;
    float offsetY;
} CharAnimDef;

typedef struct {
    float scale;
    bool flipX;
    Vector2 camera;          /* camera_position (0,0 = nao definida) */
    Vector2 position;        /* position */
    float singDuration;      /* sing_duration */
    bool hasHealthColor;     /* healthbar_colors */
    Color healthColor;
    char healthIcon[64];     /* healthicon */
    char imageBase[64];      /* image (nome do png, sem pasta/extensao) */
    CharAnimDef defs[CHAR_MAX_DEFS];
    int defCount;
} CharJson;

void Char_JsonInit(CharJson *json);
bool Char_ReadJson(const char *path, CharJson *json);
void Char_AddDefaultDefs(CharJson *json);

#endif /* CHARACTER_JSON_H */
