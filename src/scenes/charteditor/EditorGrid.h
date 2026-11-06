/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: a grade (notas, linhas de tempo, cursor, mouse) e seu layout. */
#pragma once
#ifndef EDITOR_GRID_H
#define EDITOR_GRID_H

#include "std.h"
#include "Render.h"
#include "scenes/Song.h"

// ---------- grade ----------
#define GRID_X0 40.0f

#define LANE_W 64.0f

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
float EdPxPerMs(void);
void Edit_DrawGrid(void);

#endif
