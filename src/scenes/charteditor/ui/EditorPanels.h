/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#pragma once
#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "scenes/charteditor/data/EditorDataAndIO.h"
#include "scenes/charteditor/grid/EditorTimeline.h"
#include "scenes/charteditor/audio/EditorAudio.h"
#include "scenes/charteditor/ui/EditorWidgets.h"
#include "scenes/charteditor/grid/EditorGrid.h"

void Edit_DrawMenuBar(void);
void Edit_DrawTabs(void);
void Edit_DrawPanel(void);

#endif
