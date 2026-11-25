/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef FONTLOADER_H
#define FONTLOADER_H

#include "Render.h"

/* Fonte principal da engine (assets/fonts/main.ttf). */
extern Font mainFont;

void FontLoader_LoadMain(void);
void FontLoader_UnloadMain(void);

#endif
