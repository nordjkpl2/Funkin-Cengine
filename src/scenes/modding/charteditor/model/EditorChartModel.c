/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: chart em edicao (global), criacao padrao, liberacao e mensagem de status. */

#include "scenes/modding/charteditor/model/EditorChartModel.h"
#include "scenes/modding/charteditor/model/EditorSection.h"

#include <stdlib.h>
#include <string.h>

EdChart chart;
char chartLoaded = 0;
char statusText[128];
float statusTimer = 0;

void EdChart_Free(EdChart* c) {
    if(c->secs != NULL) {
        for(int i = 0; i < c->nsec; i++)
            EdSec_Free(c->secs + i);
        free(c->secs);
        c->secs = NULL;
    }
    c->nsec = 0;
}

void EdChart_Default(EdChart* c, const char* name) {
    memset(c, 0, sizeof(*c));
    strncpy(c->name, name, sizeof(c->name) - 1);
    strncpy(c->p1, "bf", sizeof(c->p1) - 1);
    strncpy(c->p2, "dad", sizeof(c->p2) - 1);
    strncpy(c->stage, "stage", sizeof(c->stage) - 1);
    strncpy(c->gf, "gf", sizeof(c->gf) - 1);
    c->bpm = 100.0f;
    c->speed = 1.0f;
    c->secs = calloc(1, sizeof(EdSec));
    if(c->secs != NULL) {
        EdSec_Default(c->secs, 100.0f);
        c->nsec = 1;
    }
}

void EdStatusMsg(const char* m) {
    strncpy(statusText, m, sizeof(statusText) - 1);
    statusText[sizeof(statusText) - 1] = 0;
    statusTimer = 3.0f;
}
