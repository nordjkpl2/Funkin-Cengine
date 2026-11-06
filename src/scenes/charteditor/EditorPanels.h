/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#pragma once
#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "std.h"
#include "Render.h"
#include "scenes/Song.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"

void Edit_DrawMenuBar(void);
void Edit_DrawTabs(void);
void Edit_DrawPanel(void);

#endif
