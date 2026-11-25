/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Decodificador de PNG rapido (Wuffs). Seguro para threads de fundo: so CPU, sem estado global. */
#pragma once
#include "raylib.h"

// Decodifica um PNG para RGBA8 com Wuffs. Se falhar, cai no LoadImage do raylib.
// Image.data == NULL se os dois falharem. Libere com UnloadImage.
Image PngDecode_Load(const char* path);
