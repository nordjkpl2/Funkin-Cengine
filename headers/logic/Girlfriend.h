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

typedef struct {
    RayAnimatedObject object;
    unsigned char f;         /* 0/1: qual das duas poses de danca */
    int anims[2];
} Girlfriend;

void Girlfriend_Load(Girlfriend* restrict gf, const char* restrict name);

#endif /* GIRLFRIEND_H */
