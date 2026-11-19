/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * FontLoader.c - carrega e libera a fonte principal da engine.
 */

#include "scenes/api/FontLoader.h"

Font mainFont;

void FontLoader_LoadMain(void) {
    mainFont = LoadFontEx("assets/fonts/main.ttf", 64, 0, 0);
}

void FontLoader_UnloadMain(void) {
    UnloadFont(mainFont);
}
