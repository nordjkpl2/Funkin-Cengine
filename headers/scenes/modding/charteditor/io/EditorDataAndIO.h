/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: dados do chart. Umbrella dos modulos de data/ (tipos, trechos, modelo, undo,
   carga, gravacao, eventos, arquivos). Codigo novo deve incluir o modulo especifico. */
#pragma once
#ifndef EDITOR_DATA_AND_IO_H
#define EDITOR_DATA_AND_IO_H

#include "scenes/modding/charteditor/model/EditorTypes.h"
#include "scenes/modding/charteditor/model/EditorSection.h"
#include "scenes/modding/charteditor/model/EditorChartModel.h"
#include "scenes/modding/charteditor/undo/EditorUndo.h"
#include "scenes/modding/charteditor/io/EditorChartLoad.h"
#include "scenes/modding/charteditor/io/EditorChartSave.h"
#include "scenes/modding/charteditor/model/EditorEvents.h"
#include "scenes/modding/charteditor/io/EditorFiles.h"

#endif
