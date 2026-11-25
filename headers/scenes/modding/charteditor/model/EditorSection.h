/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: operacoes sobre as notas de um trecho (insercao, busca, remocao, copia). */
#pragma once
#ifndef EDITOR_SECTION_H
#define EDITOR_SECTION_H

#include "scenes/modding/charteditor/model/EditorTypes.h"

void EdSec_Default(EdSec* s, float bpm); /* trecho vazio com os padroes (turno do jogador, 16 steps) */
void EdSec_Free(EdSec* s);
void EdSec_Clone(EdSec* dst, const EdSec* src); /* copia profunda (so as notas sao alocadas) */
size_t EdSec_Bytes(const EdSec* s);

void EdSec_Add(EdSec* s, float time, int lane, float len);      /* com dedup e ordem */
void EdSec_AddBulk(EdSec* s, float time, int lane, float len);  /* anexa; fechar com FinishBulk */
void EdSec_FinishBulk(EdSec* s);                                 /* ordena + dedup */
int EdSec_LowerBound(const EdSec* s, float time);     /* 1o indice com time >= time (busca binaria) */
int EdSec_FindNear(EdSec* s, float time, int lane, float windowMs); /* indice da nota perto, ou -1 */
void EdSec_SetLen(EdSec* s, int idx, float len); /* nota longa: limita 0..10000 ms e atualiza tmax */
char EdSec_RemoveAt(EdSec* s, int idx);
int EdDispLane(const EdSec* s, int lane); /* casa da tela <-> casa do arquivo (a funcao e a propria inversa) */

#endif
