/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: undo por trecho (diario de operacoes).
 *
 * Cada passo guarda so o que a operacao muda, nao o chart inteiro:
 *   SEC  - copia de um trecho (notas + bpm/steps/turno), antes de editar o trecho.
 *   INS  - trecho apagado (guardado para reinserir na mesma posicao).
 *   ADD  - trecho novo no fim; desfazer so remove o ultimo.
 *   META - escalares do chart (bpm geral, speed...). Sem notas.
 * A pilha e LIFO, entao a estrutura de trechos no momento de desfazer e a mesma de quando o passo
 * foi gravado. Limites: UNDO_MAX passos e UNDO_BUDGET_BYTES de memoria (so os trechos contam). */

#include "scenes/modding/charteditor/data/EditorUndo.h"
#include "scenes/modding/charteditor/data/EditorChartModel.h"
#include "scenes/modding/charteditor/data/EditorSection.h"

#include <stdlib.h>
#include <string.h>

#define UNDO_MAX 50
#define UNDO_BUDGET_BYTES (128u * 1024u * 1024u)

typedef enum { UNDO_SEC, UNDO_INS, UNDO_ADD, UNDO_META } UndoKind;

typedef struct {
    UndoKind kind;
    int sec;       // indice do trecho (SEC/INS)
    EdSec data;    // trecho guardado (SEC/INS)
    EdChart head;  // escalares (META), secs = NULL
    size_t bytes;  // memoria ocupada pelo passo
} UndoEntry;

static UndoEntry undoStack[UNDO_MAX];
static int undoCount = 0;

static void UndoEntry_Release(UndoEntry* e) {
    if(e->kind == UNDO_SEC || e->kind == UNDO_INS)
        EdSec_Free(&e->data);
}

static void Undo_DropOldest(void) {
    UndoEntry_Release(undoStack);
    memmove(undoStack, undoStack + 1, sizeof(UndoEntry) * (size_t)(undoCount - 1));
    undoCount--;
}

// empilha (toma posse do conteudo de e) e aplica os limites de passos e de memoria
static void Undo_Push(const UndoEntry* e) {
    if(undoCount >= UNDO_MAX)
        Undo_DropOldest();
    undoStack[undoCount++] = *e;
    for(;;) {
        size_t total = 0;
        for(int i = 0; i < undoCount; i++) total += undoStack[i].bytes;
        if(total <= UNDO_BUDGET_BYTES || undoCount <= 1) break;
        Undo_DropOldest();
    }
}

void Undo_Clear(void) {
    for(int i = 0; i < undoCount; i++)
        UndoEntry_Release(undoStack + i);
    undoCount = 0;
}

static void PushSecCopy(UndoKind kind, int si) {
    UndoEntry e;
    memset(&e, 0, sizeof(e));
    e.kind = kind;
    e.sec = si;
    EdSec_Clone(&e.data, chart.secs + si);
    e.bytes = EdSec_Bytes(&e.data);
    Undo_Push(&e);
}

void EdTouchSec(int si) {
    if(si >= 0 && si < chart.nsec)
        PushSecCopy(UNDO_SEC, si);
    chart.dirty = 1;
}

void EdTouchDelSec(int si) {
    if(si >= 0 && si < chart.nsec)
        PushSecCopy(UNDO_INS, si);
    chart.dirty = 1;
}

void EdTouchAddSec(void) {
    UndoEntry e;
    memset(&e, 0, sizeof(e));
    e.kind = UNDO_ADD;
    e.bytes = sizeof(EdSec);
    Undo_Push(&e);
    chart.dirty = 1;
}

void EdTouchMeta(void) {
    UndoEntry e;
    memset(&e, 0, sizeof(e));
    e.kind = UNDO_META;
    e.head = chart;
    e.head.secs = NULL;
    e.head.nsec = 0;
    e.bytes = sizeof(EdChart);
    Undo_Push(&e);
    chart.dirty = 1;
}

void Undo_Pop(void) {
    if(undoCount <= 0)
        return;
    UndoEntry* e = undoStack + (--undoCount);
    switch(e->kind) {
    case UNDO_SEC:
        if(e->sec >= 0 && e->sec < chart.nsec) {
            // troca: o trecho atual vai para o passo (e e liberado abaixo)
            EdSec tmp = chart.secs[e->sec];
            chart.secs[e->sec] = e->data;
            e->data = tmp;
        }
        break;
    case UNDO_INS:
        if(e->sec >= 0 && e->sec <= chart.nsec) {
            EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
            if(g != NULL) {
                chart.secs = g;
                memmove(chart.secs + e->sec + 1, chart.secs + e->sec, sizeof(EdSec) * (size_t)(chart.nsec - e->sec));
                chart.secs[e->sec] = e->data; // posse das notas passa ao chart
                e->data.notes = NULL;
                chart.nsec++;
            }
        }
        break;
    case UNDO_ADD:
        if(chart.nsec > 0) {
            EdSec_Free(chart.secs + chart.nsec - 1);
            chart.nsec--;
        }
        break;
    case UNDO_META: {
        EdSec* keepSecs = chart.secs;
        int keepN = chart.nsec;
        chart = e->head;
        chart.secs = keepSecs;
        chart.nsec = keepN;
        break;
    }
    }
    UndoEntry_Release(e);
}
