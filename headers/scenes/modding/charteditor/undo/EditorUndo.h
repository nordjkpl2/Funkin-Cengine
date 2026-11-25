/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: undo por trecho. Ver EditorUndo.c para o modelo de passos. */
#pragma once
#ifndef EDITOR_UNDO_H
#define EDITOR_UNDO_H

/* Chamar ANTES de mudar o dado: grava so o que a operacao toca. */
void EdTouchSec(int si);    /* muda notas/bpm/steps/turno do trecho si */
void EdTouchDelSec(int si); /* vai apagar o trecho si */
void EdTouchAddSec(void);   /* vai anexar um trecho no fim */
void EdTouchMeta(void);     /* muda escalar do chart (bpm geral, speed) */

void Undo_Pop(void);
void Undo_Clear(void);

#endif
