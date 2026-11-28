/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: icone desenhado de cada tipo de evento (picker e lista de eventos da musica). */
#pragma once
#ifndef EDITOR_EVENT_ICONS_H
#define EDITOR_EVENT_ICONS_H

#include "raylib.h"

/* Fundo com a cor da categoria + desenho do evento. Nome desconhecido: sigla (iniciais). */
void EdEventIcon_Draw(Rectangle r, const char* name);

#endif
