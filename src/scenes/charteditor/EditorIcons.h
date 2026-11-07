/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: carrega e solta os icones de vida do oponente e do jogador. */
#pragma once
#ifndef EDITOR_ICONS_H
#define EDITOR_ICONS_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"

void Editor_UnloadIcons(void);
void Editor_LoadIcons(void);

#endif
