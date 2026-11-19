/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: carrega e solta os icones de vida do oponente e do jogador. */
#pragma once
#ifndef EDITOR_ICONS_H
#define EDITOR_ICONS_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "scenes/charteditor/data/EditorDataAndIO.h"
#include "scenes/charteditor/grid/EditorTimeline.h"
#include "scenes/charteditor/audio/EditorAudio.h"
#include "scenes/charteditor/ui/EditorWidgets.h"
#include "scenes/charteditor/grid/EditorGrid.h"

void Editor_UnloadIcons(void);
void Editor_LoadIcons(void);

#endif
