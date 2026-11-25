/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: acoes de edicao (trechos, undo, navegacao). Atalhos, menu e abas chamam estas,
   entao cada acao existe uma vez so (com undo e cache de tempos certos). */
#pragma once
#ifndef EDITOR_ACTIONS_H
#define EDITOR_ACTIONS_H

void EdAct_NewPart(void);        /* trecho vazio no fim (copia turno e bpm do ultimo) */
void EdAct_DeletePart(void);     /* apaga o trecho atual (nunca o ultimo que sobrou) */
void EdAct_CopyPart(void);
void EdAct_PastePart(void);      /* cola a copia como trecho novo no fim */
void EdAct_Undo(void);
void EdAct_GoToPart(int idx);    /* cursor e vista no comeco do trecho idx (limitado) */
void EdAct_ToggleTurn(void);
void EdAct_NudgeBpm(float delta); /* bpm do trecho atual += delta (10..300) */
void EdAct_CycleSnap(void);       /* grade: 1/16 -> 1/4 -> 1/8 -> 1/16 */

#endif
