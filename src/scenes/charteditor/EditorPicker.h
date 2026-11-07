/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: lista de musicas, abrir/salvar/importar e o estado da sessao de edicao. */
#pragma once
#ifndef EDITOR_PICKER_H
#define EDITOR_PICKER_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"

// ---------- estado da cena ----------
typedef enum { CE_PICKER, CE_EDIT } CeMode;

// abas estilo Psych (uma ativa por vez, resto colapsado)
typedef enum { TAB_CHARTING, TAB_DATA, TAB_EVENTS, TAB_SECTION, TAB_SONG, TAB_COUNT } ChartTab;

const char* ChartTab_Name(int t);
extern CeMode ceMode;
extern int activeTab;
extern TxtBox openBox;
extern TxtBox saveAsBox;
extern char pendingDir[256];
extern EdSec copyBuf;
extern char copyValid;
extern RayGraphicObject bgEd;
void Picker_Refresh(void);
void Picker_Free(void);
void Editor_EnterDir(const char* dir);
void Editor_Save(void);
char ImportJSON(const char* jsonPath, const char* modName);
char Editor_SaveAs(const char* modName);
void CharDrop_Draw(void);
extern char importPath[512];
void Picker_Draw(void);
void Picker_Update(void);
void Editor_LeaveToPicker(void);
void Editor_RequestLeave(void);

#endif
