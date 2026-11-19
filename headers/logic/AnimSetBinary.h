#pragma once
#ifndef ANIMSET_BINARY_H
#define ANIMSET_BINARY_H

#include <stdbool.h>
#include "Render.h"

/* Le um .animset binario e preenche out. */
bool Binary_LoadAnimSet(const char *path, Texture2D image, RayAnimationHandler *out);

#endif /* ANIMSET_BINARY_H */
