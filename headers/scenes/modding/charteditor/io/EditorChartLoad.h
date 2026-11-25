/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: carregamento do chart (JSON em streaming, formato Psych/FNF e .song legado). */
#pragma once
#ifndef EDITOR_CHART_LOAD_H
#define EDITOR_CHART_LOAD_H

#include "scenes/modding/charteditor/model/EditorTypes.h"

/* Le o JSON em streaming (sem arvore na RAM e sem limite de tamanho). Retorna 0 se nao for um chart. */
char EdChart_LoadJSON(EdChart* c, const char* jsonPath, const char* dir, const char* fallbackName);
/* Procura o chart da pasta (data.json, <pasta>.json, outro json com notas, data.song) e carrega. */
char EdChart_OpenDir(EdChart* c, const char* dir);

#endif
