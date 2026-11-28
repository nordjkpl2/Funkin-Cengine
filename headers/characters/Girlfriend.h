/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Girlfriend.h - Tipo e carregamento da namorada (GF).
 */
#pragma once
#ifndef GIRLFRIEND_H
#define GIRLFRIEND_H

#include "Render.h"

/* Papel do JSON (hey, cheer, idle-alt...) -> indice no atlas. Usado pelos eventos (Play Animation, Hey!). */
#define CHAR_NAMED_MAX 40
typedef struct {
    char role[16];
    int index;
} CharNamedAnim;

typedef struct {
    RayAnimatedObject object;
    CharNamedAnim named[CHAR_NAMED_MAX];
    int namedCount;
    unsigned char f;         /* 0/1: qual das duas poses de danca */
    int anims[2];
} Girlfriend;

void Girlfriend_Load(Girlfriend* restrict gf, const char* restrict name);

#endif /* GIRLFRIEND_H */
