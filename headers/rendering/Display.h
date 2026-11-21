/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Display.h - informacoes do monitor e swap interval (V-SYNC) da janela.
 * Fica fora do raylib de proposito: a implementacao no Windows usa windows.h.
 */
#pragma once
#ifndef RENDERING_DISPLAY_H
#define RENDERING_DISPLAY_H

#include <stdbool.h>

/* Taxa de atualizacao (Hz) do monitor onde a janela esta. hwnd = GetWindowHandle() do raylib.
   Devolve 0 se nao conseguir (quem chama usa o fallback do raylib). */
int Display_RefreshHz(void *hwnd);

/* Liga (1) ou desliga (0) o swap interval do contexto OpenGL atual. false se o driver nao aceitar. */
bool Display_SetSwapInterval(bool on);

#endif
