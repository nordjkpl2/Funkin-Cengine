/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetCharacterXML.h - Declaracao do loader de XML de personagem (Codename).
 */

#pragma once
#ifndef ANIMSET_CHARACTER_XML_H
#define ANIMSET_CHARACTER_XML_H

#include <stdbool.h>
#include "Render.h"

typedef struct RayCharacterXMLData {
    char sprite[128];

    float x;
    float y;
    float scale;
    float cameraX;
    float cameraY;
    float holdTime;
    float defaultFPS;

    Color color;

    bool isPlayer;
    bool flipX;
    bool antialiasing;

    bool hasX;
    bool hasY;
    bool hasScale;
    bool hasCameraX;
    bool hasCameraY;
    bool hasColor;

    RayAnimationHandler animations;
} RayCharacterXMLData;

/*
 * Lê um XML de personagem (character/anim) e um XML de atlas Sparrow
 * (TextureAtlas/SubTexture), construindo um RayAnimationHandler.
 * Retorna false em XML inválido, prefixo ausente, índices inválidos ou
 * erro de memória. Em caso de sucesso, `out->animations` passa a possuir
 * a textura `image`; libere com AnimationSet_FreeAll().
 */
bool AnimationSet_LoadCharacterXML(
    const char *characterXMLPath,
    const char *atlasXMLPath,
    Texture2D image,
    RayCharacterXMLData *out
);

#endif /* ANIMSET_CHARACTER_XML_H */
