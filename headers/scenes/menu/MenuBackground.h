/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * MenuBackground.h - fundo azul compartilhado pelas telas de menu (menuBGBlue.png).
 */
#pragma once
#ifndef MENUBACKGROUND_H
#define MENUBACKGROUND_H

#include "raylib.h"

/* Textura do fundo. Carrega na primeira chamada e fica residente ate MenuBackground_Free.
 * Quem recebe NAO deve chamar UnloadTexture: a textura e de todas as cenas de menu. */
Texture2D MenuBackground_Get(void);

/* Libera a textura da GPU. Chamada so no encerramento do jogo (AllScenes_DestroyGame). */
void MenuBackground_Free(void);

#endif
