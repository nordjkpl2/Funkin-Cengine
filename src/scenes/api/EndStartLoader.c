/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * EndStartLoader.c - inicio e fim do jogo.
 */

#include "scenes/api/EndStartLoader.h"
#include "scenes/api/FontLoader.h"
#include "scenes/api/NoteSkinCache.h"
#include "scenes/menu/MenuMusic.h"

Vector2 VECTOR_ZERO;

void AllScenes_StartGame(void) {
    VECTOR_ZERO = (Vector2) {0, 0};

    MenuMusic_Init();   // faixa salva ou Alpha (assets/music/menu)
    FontLoader_LoadMain();

    NoteSkinCache_Reset(); // zera o cache de skins
}

void AllScenes_DestroyGame(void) {
    NoteSkinCache_Free();
    FontLoader_UnloadMain();
}
