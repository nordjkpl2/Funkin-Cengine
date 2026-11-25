/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: gravacao do chart (data.json) em streaming. */
#pragma once
#ifndef EDITOR_CHART_SAVE_H
#define EDITOR_CHART_SAVE_H

#include "scenes/modding/charteditor/model/EditorTypes.h"

char EdChart_Save(const EdChart* c);           /* formatado (uma nota por linha) */
char EdChart_SaveCompressed(const EdChart* c); /* minificado, campos padrao omitidos */
char EdChart_SaveBinary(const EdChart* c);     /* data.song binario na pasta da musica (formato legado) */

#endif
