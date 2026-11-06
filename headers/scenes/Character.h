/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl, outd day19 of10 2026 */
#pragma once
#ifndef CHARACTER_H
#define CHARACTER_H

#include "Render.h"
#include <stdbool.h>

/* Quem e o personagem: define animacoes padrao, camera e icone de reserva.
   CHAR_GF e de uso interno (Girlfriend_Load). */
typedef enum {
    CHAR_BF = 0,
    CHAR_DAD = 1,
    CHAR_GF = 2
} CharRole;

typedef struct {
    RayAnimatedObject object;
    Vector2 cameraOffset;
    Texture2D icon;          /* icone da barra de vida */
    float idleTimer;
    bool idled;
    struct { int idle; int notes[4]; } animations;
    struct { int notes[4]; } missAnimations;
    float singDuration;      /* passos que segura a pose de cantar (Psych; padrao 4) */
    bool hasHealthColor;     /* healthbar_colors do JSON do personagem */
    Color healthColor;
} Character;

typedef struct {
    RayAnimatedObject object;
    unsigned char f;         /* 0/1: qual das duas poses de danca */
    int anims[2];
} Girlfriend;

/* Define a pasta do mod da musica (assets/mods/<mod>); chamar antes de carregar. */
void Character_SetSongDir(const char* restrict songDir);

/* Busca no mod da musica primeiro, depois em assets/. Sem arquivos, cai no bf/dad padrao. */
void Character_Load(Character* restrict character, const char* restrict characterName, CharRole role);
void Girlfriend_Load(Girlfriend* restrict gf, const char* restrict name);

/* anims[0..2] = morrer, loop, confirmar (usa a idle quando o personagem nao tem). */
void Character_LoadDeathAnimations(Character* restrict character, int* restrict anims);

#endif
