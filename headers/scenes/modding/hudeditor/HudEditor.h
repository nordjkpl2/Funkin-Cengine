/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * HudEditor.h - HUD Editor: layout do HUD do gameplay (assets/hud.json via HudConfig).
 * Roda como modo do hub de modding (ModdingMenu.c), nao como cena propria.
 */
#pragma once
#ifndef HUDEDITOR_H
#define HUDEDITOR_H

void Hud_Enter(void);
char Hud_Update(void); /* 1 = sair para a lista do hub */
void Hud_Draw(void);
void Hud_Leave(void);

#endif
