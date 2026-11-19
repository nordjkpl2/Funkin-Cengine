#pragma once
#ifndef ANIMSET_SPARROW_H
#define ANIMSET_SPARROW_H

#include <stdbool.h>
#include "Render.h"

/* Le um atlas Sparrow (TextureAtlas/SubTexture) e preenche out. */
bool Atlas_LoadSparrowXML(const char *path, Texture2D image, RayAnimationHandler *out);

#endif /* ANIMSET_SPARROW_H */
