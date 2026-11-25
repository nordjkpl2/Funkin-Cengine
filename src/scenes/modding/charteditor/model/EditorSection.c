/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: operacoes sobre as notas de um trecho. */

#include "scenes/modding/charteditor/model/EditorSection.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int EdNote_Cmp(const void* a, const void* b) {
    float ta = ((const EdNote*)a)->time;
    float tb = ((const EdNote*)b)->time;
    if(ta < tb) return -1;
    if(ta > tb) return 1;
    return 0;
}

static void EdSec_Sort(EdSec* s) {
    if(s->count > 1)
        qsort(s->notes, (size_t)s->count, sizeof(EdNote), EdNote_Cmp);
}

// atualiza os limites de tempo das notas do trecho (usado pra nao desenhar o que esta fora da tela)
static void EdSec_Grow(EdSec* s, float time, float len) {
    if(len < 0) len = 0;
    const float end = time + len;
    if(s->count == 0 || time < s->tmin) s->tmin = time;
    if(s->count == 0 || end > s->tmax) s->tmax = end;
    if(s->count == 0 || len > s->lenMax) s->lenMax = len;
}

/* As notas de um trecho ficam sempre ordenadas por tempo: a busca e binaria, entao charts com
   milhoes de notas num trecho nao varrem tudo a cada clique. */
int EdSec_LowerBound(const EdSec* s, float time) { /* 1o indice com time >= time */
    int lo = 0, hi = s->count;
    while(lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if(s->notes[mid].time < time) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int EdSec_UpperBound(const EdSec* s, float time) { /* 1o indice com time > time */
    int lo = 0, hi = s->count;
    while(lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if(s->notes[mid].time <= time) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* Proxima capacidade (dobra) sem estourar int. Retorna 0 se ja nao ha como crescer. */
static int EdSec_NextCap(int cap, int first) {
    if(cap == 0) return first;
    if(cap > INT_MAX / 2) return cap < INT_MAX ? INT_MAX : 0;
    return cap * 2;
}

void EdSec_Default(EdSec* s, float bpm) {
    memset(s, 0, sizeof(*s));
    s->mustHit = 1;
    s->bpm = bpm;
    s->changeBPM = 1;
    s->lenSteps = 16;
}

void EdSec_Free(EdSec* s) {
    free(s->notes);
    s->notes = NULL;
    s->count = 0;
    s->cap = 0;
}

void EdSec_Clone(EdSec* dst, const EdSec* src) {
    *dst = *src;
    dst->notes = NULL;
    dst->count = 0;
    dst->cap = 0;
    if(src->count > 0) {
        dst->notes = malloc(sizeof(EdNote) * (size_t)src->count);
        if(dst->notes == NULL)
            return;
        memcpy(dst->notes, src->notes, sizeof(EdNote) * (size_t)src->count);
        dst->count = src->count;
        dst->cap = src->count;
    }
}

size_t EdSec_Bytes(const EdSec* s) {
    return sizeof(EdSec) + sizeof(EdNote) * (size_t)s->count;
}

void EdSec_Add(EdSec* s, float time, int lane, float len) {
    // tira duplicada (mesma casa, <2ms) igual ao jogo: so os vizinhos dentro de 2 ms importam
    for(int i = EdSec_LowerBound(s, time - 2.0f); i < s->count && s->notes[i].time < time + 2.0f; i++) {
        if(s->notes[i].lane == lane && fabsf(s->notes[i].time - time) < 2.0f)
            return;
    }
    if(s->count >= s->cap) {
        int nc = EdSec_NextCap(s->cap, 8);
        EdNote* g = nc > 0 ? realloc(s->notes, sizeof(EdNote) * (size_t)nc) : NULL;
        if(g == NULL) return;
        s->notes = g;
        s->cap = nc;
    }
    EdSec_Grow(s, time, len);
    const int pos = EdSec_UpperBound(s, time);
    memmove(s->notes + pos + 1, s->notes + pos, sizeof(EdNote) * (size_t)(s->count - pos));
    s->notes[pos].time = time;
    s->notes[pos].lane = lane;
    s->notes[pos].len = (len < 0) ? 0 : len;
    s->count++;
}

// bulk: anexa sem dedup/sort (import/paste/carga em massa). Fecha com FinishBulk.
void EdSec_AddBulk(EdSec* s, float time, int lane, float len) {
    if(s->count >= s->cap) {
        int nc = EdSec_NextCap(s->cap, 1024);
        EdNote* g = nc > 0 ? realloc(s->notes, sizeof(EdNote) * (size_t)nc) : NULL;
        if(g == NULL) return;
        s->notes = g;
        s->cap = nc;
    }
    EdSec_Grow(s, time, len);
    s->notes[s->count].time = time;
    s->notes[s->count].lane = lane;
    s->notes[s->count].len = (len < 0) ? 0 : len;
    s->count++;
}

// ordena + dedup adjacente (mesma casa <2ms). Equivale ao Add um-a-um:
// contra o set ja filtrado, so o vizinho anterior da mesma casa importa.
void EdSec_FinishBulk(EdSec* s) {
    if(s->count < 2) return;
    EdSec_Sort(s);
    float lastT[8];
    char seen[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i = 0; i < 8; i++)
        lastT[i] = -1000000.0f;
    int w = 0;
    for(int r = 0; r < s->count; r++) {
        int lane = s->notes[r].lane;
        if(lane >= 0 && lane < 8) {
            if(seen[lane] && fabsf(s->notes[r].time - lastT[lane]) < 2.0f)
                continue;
            seen[lane] = 1;
            lastT[lane] = s->notes[r].time;
        }
        if(w != r)
            s->notes[w] = s->notes[r];
        w++;
    }
    s->count = w;
}

// lado da tela: no editor VOCE fica sempre a direita (igual ao jogo).
int EdDispLane(const EdSec* s, int lane) {
    if(s->mustHit)
        return (lane < 4) ? lane + 4 : lane - 4;
    return lane;
}

int EdSec_FindNear(EdSec* s, float time, int lane, float windowMs) {
    int best = -1;
    float bestD = windowMs;
    for(int i = EdSec_LowerBound(s, time - windowMs); i < s->count; i++) {
        if(s->notes[i].time - time > windowMs)
            break;
        if(s->notes[i].lane != lane)
            continue;
        float d = fabsf(s->notes[i].time - time);
        if(d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void EdSec_SetLen(EdSec* s, int idx, float len) {
    if(idx < 0 || idx >= s->count)
        return;
    EdNote* n = s->notes + idx;
    n->len = fminf(fmaxf(len, 0.0f), 10000.0f);
    /* rastro maior: o limite do trecho cresce junto, senao a cauda some quando a cabeca sai da tela */
    if(n->time + n->len > s->tmax)
        s->tmax = n->time + n->len;
    if(n->len > s->lenMax)
        s->lenMax = n->len;
}

char EdSec_RemoveAt(EdSec* s, int idx) {
    if(idx < 0 || idx >= s->count)
        return 0;
    memmove(s->notes + idx, s->notes + idx + 1, sizeof(EdNote) * (size_t)(s->count - idx - 1));
    s->count--;
    return 1;
}
