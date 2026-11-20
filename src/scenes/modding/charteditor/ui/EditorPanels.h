/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#pragma once
#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "scenes/modding/charteditor/data/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"

#include "scenes/gameplay/PlayState.h"

void Edit_DrawMenuBar(void);
void Edit_DrawTabs(void);
void Edit_DrawPanel(void);
/* Salva e abre o test play no modo dado (F5 = FULL, F6 = RECEPTORS). */
void Edit_StartTest(PlayStateTestMode mode);

#endif
