/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * MenuBackground.c - fundo azul compartilhado pelas telas de menu.
 *
 * Decisao de ciclo de vida: a troca de cena destroi a anterior antes de criar a nova
 * (RayGame_ClearScene), entao uma contagem de referencias cairia a zero entre elas e
 * a textura seria recarregada do disco a cada entrada. Por isso fica residente (~3,7 MB
 * de VRAM para 1286x730 RGBA) e so e liberada no encerramento do jogo.
 */
#include "scenes/menu/MenuBackground.h"
#include "Render.h"

#define MENU_BACKGROUND_PATH "assets/images/freeplay/menuBGBlue.png"

static Texture2D background;
static char loaded;

Texture2D MenuBackground_Get(void)
{
    if (!loaded) {
        background = Render_LoadTexture(MENU_BACKGROUND_PATH);
        loaded = background.id != 0;
    }
    return background;
}

void MenuBackground_Free(void)
{
    if (loaded && background.id != 0)
        UnloadTexture(background);
    background = (Texture2D){0};
    loaded = 0;
}
