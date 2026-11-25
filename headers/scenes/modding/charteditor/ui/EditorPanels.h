/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: barra de menu, abas e paineis laterais (Song, Section, Note...). */
#pragma once
#ifndef EDITOR_PANELS_H
#define EDITOR_PANELS_H

#include "std.h"
#include "Render.h"
#include "data/song/Song.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"

#include "scenes/gameplay/PlayState.h"
#include "scenes/gameplay/events/PlayStateEventsInGameplay.h"
#include "rendering/shaders/ShaderFx.h"

void Edit_DrawMenuBar(void);
void Edit_DrawTabs(void);
void Edit_DrawPanel(void);
/* TAB: abre/fecha o overlay de atalhos (fundo preto). */
void Edit_ToggleHelp(void);
void Edit_DrawHelpOverlay(void);
/* Preview do shader do evento selecionado (chamar no update do editor). */
void Edit_ShaderPreview(void);
/* Roda do mouse sobre a lista do seletor de eventos: rola a lista e retorna 1 (grade nao rola). */
char Edit_EventPickerWheel(float wheel);
/* Roda do mouse sobre a lista de eventos da musica: rola a lista e retorna 1. */
char Edit_EventListWheel(float wheel);
/* Salva e abre o test play no modo dado (F5 = FULL, F6 = RECEPTORS). */
void Edit_StartTest(PlayStateTestMode mode);

#endif
