/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: chart em edicao (global), criacao padrao, liberacao e mensagem de status. */
#pragma once
#ifndef EDITOR_CHART_MODEL_H
#define EDITOR_CHART_MODEL_H

#include "scenes/modding/charteditor/model/EditorTypes.h"

extern EdChart chart;
extern char chartLoaded;
extern char statusText[128];
extern float statusTimer;

void EdChart_Free(EdChart* c); /* libera as secoes e zera nsec (o cabecalho fica) */
void EdChart_Default(EdChart* c, const char* name);
void EdStatusMsg(const char* m);

#endif
