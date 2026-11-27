/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: a grade (notas, linhas de tempo, cursor, mouse) e seu layout. */
#pragma once
#ifndef EDITOR_GRID_H
#define EDITOR_GRID_H

#include "std.h"
#include "Render.h"
#include "data/song/Song.h"

// ---------- grade ----------
#define GRID_X0 40.0f

#define LANE_W 64.0f

#define GRID_W (LANE_W * 8)

#define GRID_Y0 112.0f

#define GRID_Y1 680.0f

#define VIEW_MS 2200.0

#define MENU_H 28.0f

#define PANEL_X 586.0f

extern Texture2D edIconO;
extern Texture2D edIconP;
extern char edIconOOk;
extern char edIconPOk;
void EdArrows_Init(void);
void EdArrows_Reset(void);   // refaz os sprites das notas a partir da skin atual (Note_GetCurrentSkin)
void EdArrows_Free(void);    // solta atlas/shader das cores (ao sair da cena)
void EdArrows_DrawIcon(int lane, float cx, float cy, float size, Color tint);   // icone de seta (menus)
float EdPxPerMs(void);
int EdGrid_LaneAt(Vector2 m);   /* casa da tela (0..7) sob o ponto, ou -1 fora da grade */
double EdGrid_TimeAt(float y);  /* tempo (ms) na altura y da grade */
void Edit_DrawGrid(void);

#endif
