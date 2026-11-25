/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: acoes de edicao compartilhadas por atalhos, menu e abas. */

#include "scenes/modding/charteditor/EditorActions.h"
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"
#include "scenes/modding/charteditor/grid/EditorGrid.h"
#include "scenes/modding/charteditor/ui/EditorPicker.h"

#include <math.h>

/* Cresce chart.secs em 1 e devolve o trecho novo (zerado), ou NULL sem memoria.
   O undo so e gravado depois do realloc: se falhar, desfazer nao apaga um trecho real. */
static EdSec* Part_Append(void) {
    EdSec* g = realloc(chart.secs, sizeof(EdSec) * (size_t)(chart.nsec + 1));
    if(g == NULL) {
        EdStatusMsg("OUT OF MEMORY");
        return NULL;
    }
    chart.secs = g;
    EdTouchAddSec();
    EdSec* n = chart.secs + chart.nsec;
    memset(n, 0, sizeof(*n));
    chart.nsec++;
    SecTimes_Invalidate();
    return n;
}

void EdAct_GoToPart(int idx) {
    if(idx >= chart.nsec) idx = chart.nsec - 1;
    if(idx < 0) idx = 0;
    secIdx = idx;
    cursorMs = SecStartMs(idx);
    viewTopMs = fmax(cursorMs - VIEW_MS * 0.3, 0.0);
}

void EdAct_NewPart(void) {
    if(chart.nsec <= 0)
        return;
    const EdSec last = chart.secs[chart.nsec - 1];   /* copia: o realloc pode mover */
    EdSec* n = Part_Append();
    if(n == NULL)
        return;
    n->mustHit = last.mustHit;
    n->bpm = last.bpm;
    n->lenSteps = 16;
    EdAct_GoToPart(chart.nsec - 1);
    EdStatusMsg("NEW PART AT END");
}

void EdAct_DeletePart(void) {
    if(chart.nsec <= 1) {
        EdStatusMsg("CANNOT DELETE: ONLY 1 PART");
        return;
    }
    EdTouchDelSec(secIdx);
    free(chart.secs[secIdx].notes);
    memmove(chart.secs + secIdx, chart.secs + secIdx + 1, sizeof(EdSec) * (size_t)(chart.nsec - secIdx - 1));
    chart.nsec--;
    if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
    SecTimes_Invalidate();
    EdStatusMsg("PART DELETED");
}

void EdAct_CopyPart(void) {
    if(copyValid) EdSec_Free(&copyBuf);
    EdSec_Clone(&copyBuf, chart.secs + secIdx);
    copyValid = 1;
    EdStatusMsg("PART COPIED");
}

void EdAct_PastePart(void) {
    if(!copyValid) {
        EdStatusMsg("COPY FIRST (C)");
        return;
    }
    const double base = SongEndMs();
    const double srcStart = SecStartMs(secIdx);
    const float bpm = chart.secs[chart.nsec - 1].bpm;
    EdSec* n = Part_Append();
    if(n == NULL)
        return;
    n->mustHit = copyBuf.mustHit;
    n->bpm = bpm;
    n->lenSteps = copyBuf.lenSteps;
    /* a copia ja vem ordenada e sem duplicadas: anexa direto (o deslocamento mantem a ordem) */
    for(int i = 0; i < copyBuf.count; i++) {
        const EdNote* c = copyBuf.notes + i;
        EdSec_AddBulk(n, (float)(base + (c->time - srcStart)), c->lane, c->len);
    }
    EdAct_GoToPart(chart.nsec - 1);
    EdStatusMsg("PASTED AT END");
}

void EdAct_Undo(void) {
    Undo_Pop();
    if(secIdx >= chart.nsec) secIdx = chart.nsec - 1;
    if(secIdx < 0) secIdx = 0;
    SecTimes_Invalidate();
    chart.dirty = 1;
    EdStatusMsg("ACTION REVERTED");
}

void EdAct_ToggleTurn(void) {
    EdSec* s = chart.secs + secIdx;
    EdTouchSec(secIdx);
    s->mustHit = !s->mustHit;
    EdStatusMsg(s->mustHit ? "TURN: YOU" : "TURN: OPPONENT");
}

void EdAct_NudgeBpm(float delta) {
    EdSec* s = chart.secs + secIdx;
    const float v = fminf(fmaxf(s->bpm + delta, 10.0f), 300.0f);
    if(v == s->bpm)
        return;
    EdTouchSec(secIdx);
    s->bpm = v;
    SecTimes_Invalidate();   /* o comeco dos trechos seguintes muda */
}

void EdAct_CycleSnap(void) {
    snapDiv = (snapDiv == 4) ? 1 : (snapDiv == 1) ? 2 : 4;
}
