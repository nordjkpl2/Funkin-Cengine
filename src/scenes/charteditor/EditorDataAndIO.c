/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: dados do chart (notas, trechos, undo) e leitura/gravacao em arquivo. */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include"yyjson.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorDataAndIO.h"

// helpers yyjson -> corrige bug do int vs double
static inline double yy_to_double(yyjson_val *v){
    if(yyjson_is_real(v)) return yyjson_get_real(v);
    if(yyjson_is_sint(v)) return (double)yyjson_get_sint(v);
    if(yyjson_is_uint(v)) return (double)yyjson_get_uint(v);
    return 0;
}

static inline int yy_to_int(yyjson_val *v){
    if(yyjson_is_sint(v)) return (int)yyjson_get_sint(v);
    if(yyjson_is_uint(v)) return (int)yyjson_get_uint(v);
    if(yyjson_is_real(v)) return (int)yyjson_get_real(v);
    return 0;
}

EdChart chart;

char chartLoaded = 0;

// ---------- undo (pilha de copias) ----------
#define UNDO_MAX 50

static EdChart undoStack[UNDO_MAX];

static int undoCount = 0;

/*============
 * // ^ 1. Chart
 *=====================*/

void EdChart_Free(EdChart* c) {
    if(c->secs != NULL) {
        for(int i = 0; i < c->nsec; i++)
            free(c->secs[i].notes);
        free(c->secs);
        c->secs = NULL;
    }
    c->nsec = 0;
}

static void EdChart_CopyInto(EdChart* dst, const EdChart* src) {
    EdChart_Free(dst);
    *dst = *src; // copia escalares + dir/name
    dst->secs = NULL;
    dst->nsec = 0;
    if(src->nsec > 0) {
        dst->secs = malloc(sizeof(EdSec) * (size_t)src->nsec);
        dst->nsec = src->nsec;
        for(int i = 0; i < src->nsec; i++) {
            dst->secs[i] = src->secs[i];
            dst->secs[i].notes = NULL;
            dst->secs[i].count = 0;
            dst->secs[i].cap = 0;
            if(src->secs[i].count > 0) {
                dst->secs[i].notes = malloc(sizeof(EdNote) * (size_t)src->secs[i].count);
                dst->secs[i].cap = src->secs[i].count;
                dst->secs[i].count = src->secs[i].count;
                memcpy(dst->secs[i].notes, src->secs[i].notes, sizeof(EdNote) * (size_t)src->secs[i].count);
            }
        }
    }
}

static void Undo_Push(void) {
    if(undoCount >= UNDO_MAX) {
        EdChart_Free(undoStack);
        memmove(undoStack, undoStack + 1, sizeof(EdChart) * (UNDO_MAX - 1));
        undoCount = UNDO_MAX - 1;
    }
    memset(undoStack + undoCount, 0, sizeof(EdChart));
    EdChart_CopyInto(undoStack + undoCount, &chart);
    undoCount++;
}

void Undo_Clear(void) {
    for(int i = 0; i < undoCount; i++)
        EdChart_Free(undoStack + i);
    undoCount = 0;
}

void Undo_Pop(void) {
    if(undoCount <= 0)
        return;
    undoCount--;
    EdChart_Free(&chart);
    memset(&chart, 0, sizeof(chart));
    EdChart_CopyInto(&chart, undoStack + undoCount);
    EdChart_Free(undoStack + undoCount);
}

// ---------- notas ----------
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
    float end = time + (len > 0 ? len : 0);
    if(s->count == 0 || time < s->tmin) s->tmin = time;
    if(s->count == 0 || end > s->tmax) s->tmax = end;
}

void EdSec_Add(EdSec* s, float time, int lane, float len) {
    // tira duplicada (mesma casa, <2ms) igual ao jogo
    for(int i = 0; i < s->count; i++) {
        if(s->notes[i].lane == lane && fabsf(s->notes[i].time - time) < 2.0f)
            return;
    }
    if(s->count >= s->cap) {
        int nc = (s->cap == 0) ? 8 : s->cap * 2;
        EdNote* g = realloc(s->notes, sizeof(EdNote) * (size_t)nc);
        if(g == NULL) return;
        s->notes = g;
        s->cap = nc;
    }
    EdSec_Grow(s, time, len);
    s->notes[s->count].time = time;
    s->notes[s->count].lane = lane;
    s->notes[s->count].len = (len < 0) ? 0 : len;
    s->count++;
    EdSec_Sort(s);
}

// bulk: anexa sem dedup/sort (import/paste em massa). Fecha com FinishBulk.
void EdSec_AddBulk(EdSec* s, float time, int lane, float len) {
    if(s->count >= s->cap) {
        int nc = (s->cap == 0) ? 1024 : s->cap * 2;
        EdNote* g = realloc(s->notes, sizeof(EdNote) * (size_t)nc);
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
static void EdSec_FinishBulk(EdSec* s) {
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
// vira a casa pra exibir; a funcao e a propria inversa (vale pros 2 lados)
int EdDispLane(const EdSec* s, int lane) {
    if(s->mustHit)
        return (lane < 4) ? lane + 4 : lane - 4;
    return lane;
}

// indice da nota perto, ou -1
int EdSec_FindNear(EdSec* s, float time, int lane, float windowMs) {
    int best = -1;
    float bestD = windowMs;
    for(int i = 0; i < s->count; i++) {
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

char EdSec_RemoveAt(EdSec* s, int idx) {
    if(idx < 0 || idx >= s->count)
        return 0;
    memmove(s->notes + idx, s->notes + idx + 1, sizeof(EdNote) * (size_t)(s->count - idx - 1));
    s->count--;
    return 1;
}

// ---------- carregar ----------
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
    c->nsec = 1;
    c->secs[0].mustHit = 1;
    c->secs[0].bpm = 100.0f;
    c->secs[0].changeBPM = 1;
    c->secs[0].lenSteps = 16;
}

// json FNF cru (lendo lanes 0-7 direto, sem converter)
char EdChart_LoadJSON(EdChart* c, const char* jsonPath, const char* dir, const char* fallbackName) {
    FILE* f = fopen(jsonPath, "rb");
    if(f == NULL)
        return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0 || size > 8 * 1024 * 1024) {
        fclose(f);
        return 0;
    }
    char* buf = malloc((size_t)size + 1);
    if(buf == NULL) {
        fclose(f);
        return 0;
    }
    if(fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return 0;
    }
    buf[size] = 0;
    fclose(f);

    yyjson_doc* doc = yyjson_read(buf, (size_t)size, 0);
    free(buf);
    if(doc == NULL)
        return 0;
        
    yyjson_val* root = yyjson_doc_get_root(doc);
    if(root == NULL) {
        yyjson_doc_free(doc);
        return 0;
    }

    yyjson_val* js = yyjson_obj_get(root, "song");
    if(js == NULL)
        js = root;

    yyjson_val* jnotes = yyjson_obj_get(js, "notes");
    if(!yyjson_is_arr(jnotes)) {
        yyjson_doc_free(doc);
        return 0;
    }

    EdChart tmp;
    EdChart_Default(&tmp, fallbackName);
    strncpy(tmp.dir, dir, sizeof(tmp.dir) - 1);

    yyjson_val* j = yyjson_obj_get(js, "song");
    if(yyjson_is_str(j)) strncpy(tmp.name, yyjson_get_str(j), sizeof(tmp.name) - 1);

    j = yyjson_obj_get(js, "bpm");
    if(yyjson_is_num(j) && yy_to_double(j) >= 10) tmp.bpm = (float)yy_to_double(j);
    
    j = yyjson_obj_get(js, "speed");
    if(yyjson_is_num(j) && yy_to_double(j) > 0) tmp.speed = (float)yy_to_double(j);
    
    j = yyjson_obj_get(js, "player1");
    if(yyjson_is_str(j)) strncpy(tmp.p1, yyjson_get_str(j), sizeof(tmp.p1) - 1);
    
    j = yyjson_obj_get(js, "player2");
    if(yyjson_is_str(j)) strncpy(tmp.p2, yyjson_get_str(j), sizeof(tmp.p2) - 1);
    
    j = yyjson_obj_get(js, "stage");
    if(yyjson_is_str(j)) strncpy(tmp.stage, yyjson_get_str(j), sizeof(tmp.stage) - 1);
    
    j = yyjson_obj_get(js, "gfVersion");
    if(yyjson_is_str(j)) strncpy(tmp.gf, yyjson_get_str(j), sizeof(tmp.gf) - 1);
    
    // noteSkin: custom note skin name
    j = yyjson_obj_get(js, "noteSkin");
    if(yyjson_is_str(j)) strncpy(tmp.noteSkin, yyjson_get_str(j), sizeof(tmp.noteSkin) - 1);
    else
        tmp.noteSkin[0] = 0;
    // Psych: arrowSkin (imagem em images/) vira "@<imagem>" no nosso noteSkin
    if(tmp.noteSkin[0] == 0) {
        j = yyjson_obj_get(js, "arrowSkin");
        if(yyjson_is_str(j) && yyjson_get_str(j)[0] != 0 && strcmp(yyjson_get_str(j), "NOTE_assets") != 0 &&
           strlen(yyjson_get_str(j)) + 1 < sizeof(tmp.noteSkin))
            snprintf(tmp.noteSkin, sizeof(tmp.noteSkin), "@%s", yyjson_get_str(j));
    }

    int nsec = (int)yyjson_arr_size(jnotes);
    if(nsec > 0) {
        EdChart_Free(&tmp);
        tmp.secs = calloc((size_t)nsec, sizeof(EdSec));
        tmp.nsec = nsec;
        float runBpm = tmp.bpm;
        
        for(int i = 0; i < nsec; i++) {
            yyjson_val* jsec = yyjson_arr_get(jnotes, i);
            EdSec* s = tmp.secs + i;
            s->lenSteps = 16;
            s->bpm = runBpm;
            
            if(!yyjson_is_obj(jsec))
                continue;
                
            yyjson_val* jl = yyjson_obj_get(jsec, "lengthInSteps");
            if(jl == NULL) jl = yyjson_obj_get(jsec, "lengthinsteps");
            if(yyjson_is_num(jl) && yyjson_get_real(jl) >= 1) s->lenSteps = (int)yyjson_get_int(jl);
            
            yyjson_val* jmust = yyjson_obj_get(jsec, "mustHitSection");
            if(yyjson_is_bool(jmust) && yyjson_get_bool(jmust)) 
                s->mustHit = 1;
            else 
                s->mustHit = 0;

            yyjson_val* jb = yyjson_obj_get(jsec, "bpm");
            yyjson_val* jchange = yyjson_obj_get(jsec, "changeBPM");

            s->changeBPM = 0;

            if (yyjson_is_bool(jchange) &&
                yyjson_get_bool(jchange) &&
                yyjson_is_num(jb) &&
                    yyjson_get_real(jb) >= 10.0) {

            runBpm = (float)yyjson_get_real(jb);
            s->changeBPM = 1;
            }

s->bpm = runBpm;
            
            yyjson_val* jarr = yyjson_obj_get(jsec, "sectionNotes");
            if(!yyjson_is_arr(jarr))
                continue;
                
            int rn = (int)yyjson_arr_size(jarr);
            for(int k = 0; k < rn; k++) {
                yyjson_val* jn = yyjson_arr_get(jarr, k);
                if(!yyjson_is_arr(jn) || yyjson_arr_size(jn) < 2)
                    continue;
                    
                yyjson_val* jt = yyjson_arr_get(jn, 0);
                yyjson_val* ji = yyjson_arr_get(jn, 1);
                yyjson_val* jln = yyjson_arr_size(jn) > 2 ? yyjson_arr_get(jn, 2) : NULL;
                
                if(yyjson_is_num(jl) && yy_to_double(jl) >= 1) s->lenSteps = yy_to_int(jl);
                int lane = yy_to_int(ji) % 8; // agora funciona pra 0 e 0.0
                float len = (yyjson_is_num(jln) && yy_to_double(jln) > 0) ? (float)yy_to_double(jln) : 0;

                EdSec_AddBulk(s, (float)yy_to_double(jt), lane, len); // tempo correto
            }
            EdSec_FinishBulk(s);
        }
    }

    yyjson_doc_free(doc);
    EdChart_Free(c);
    *c = tmp;
    return 1;
}

// binario .song antigo -> converte ids de volta pra lanes 0-7
static char EdChart_LoadBinary(EdChart* c, const char* binPath, const char* dir, const char* fallbackName) {
    Song s;
    memset(&s, 0, sizeof(s));
    // Song_Parse sai do jogo se o arquivo for invalido; checa antes
    FILE* t = fopen(binPath, "rb");
    if(t == NULL)
        return 0;
    fclose(t);
    Song_Parse(&s, binPath);

    EdChart tmp;
    EdChart_Default(&tmp, fallbackName);
    strncpy(tmp.dir, dir, sizeof(tmp.dir) - 1);
    strncpy(tmp.p1, s.player1, sizeof(tmp.p1) - 1);
    strncpy(tmp.p2, s.player2, sizeof(tmp.p2) - 1);
    strncpy(tmp.stage, s.stage, sizeof(tmp.stage) - 1);
    strncpy(tmp.gf, s.gfVersion, sizeof(tmp.gf) - 1);
    strncpy(tmp.noteSkin, s.noteSkin, sizeof(tmp.noteSkin) - 1);
    if(tmp.noteSkin[0] == 0 && s.arrowSkin[0] != 0 && strcmp(s.arrowSkin, "NOTE_assets") != 0 &&
       strlen(s.arrowSkin) + 1 < sizeof(tmp.noteSkin))
        snprintf(tmp.noteSkin, sizeof(tmp.noteSkin), "@%s", s.arrowSkin);
    tmp.speed = (s.speed > 0) ? s.speed : 1.0f;
    tmp.bpm = (s.sectionCount > 0) ? s.sections[0].bpm : 100.0f;

    if(s.sectionCount > 0) {
        EdChart_Free(&tmp);
        tmp.secs = calloc((size_t)s.sectionCount, sizeof(EdSec));
        tmp.nsec = s.sectionCount;
        for(int i = 0; i < s.sectionCount; i++) {
            Section* src = s.sections + i;
            EdSec* dst = tmp.secs + i;
            dst->mustHit = src->mustHit;
            dst->bpm = src->bpm;
            dst->changeBPM = (i == 0) || (src->bpm != s.sections[i - 1].bpm);
            dst->lenSteps = (src->len >= 1) ? src->len : 16;
            for(int k = 0; k < src->noteCount; k++) {
                DataNote* dn = src->notes + k;
                int lane;
                if(src->mustHit) {
                    lane = (dn->id < 0) ? (-dn->id - 1) : (dn->id + 3);
                } else {
                    lane = (dn->id > 0) ? (dn->id - 1) : (3 - dn->id);
                }
                if(lane < 0 || lane > 7)
                    continue;
                EdSec_AddBulk(dst, dn->time, lane, dn->len);
            }
            EdSec_FinishBulk(dst);
        }
    }

    Song_Free(&s);
    EdChart_Free(c);
    *c = tmp;
    return 1;
}

const char* EdDirName(const char* path) {
    const char* a = strrchr(path, '/');
    const char* b = strrchr(path, '\\');
    const char* s = (a > b) ? a : b;
    return (s != NULL) ? s + 1 : path;
}

static char EdLooksLikeChart(const char* path) {
    FILE* f = fopen(path, "rb");
    if(f == NULL) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0 || size > 8 * 1024 * 1024) { fclose(f); return 0; }
    char* buf = malloc((size_t)size + 1);
    if(buf == NULL) { fclose(f); return 0; }
    if(fread(buf, 1, (size_t)size, f) != (size_t)size) { free(buf); fclose(f); return 0; }
    buf[size] = 0;
    fclose(f);

    yyjson_doc* doc = yyjson_read(buf, (size_t)size, 0);
    free(buf);
    if(doc == NULL) return 0;

    yyjson_val* root = yyjson_doc_get_root(doc);
    if(root == NULL) { yyjson_doc_free(doc); return 0; }

    yyjson_val* js = yyjson_obj_get(root, "song");
    if(js == NULL) js = root;

    yyjson_val* jn = yyjson_obj_get(js, "notes");
    char ok = (yyjson_is_arr(jn) && yyjson_arr_size(jn) > 0);

    yyjson_doc_free(doc);
    return ok;
}

char EdChart_OpenDir(EdChart* c, const char* dir) {
    char p[300];
    snprintf(p, sizeof(p), "%s/data.json", dir);
    if(EdChart_LoadJSON(c, p, dir, EdDirName(dir)))
        return 1;
    // Psych: <pasta>.json (milf/milf.json)
    snprintf(p, sizeof(p), "%s/%s.json", dir, EdDirName(dir));
    if(EdLooksLikeChart(p) && EdChart_LoadJSON(c, p, dir, EdDirName(dir)))
        return 1;
    // qualquer outro *.json que pareca chart (satin-panties-*.json, etc)
    if(DirectoryExists(dir)) {
        FilePathList list = LoadDirectoryFilesEx(dir, ".json", false);
        for(unsigned int i = 0; i < list.count; i++) {
            const char* jp = list.paths[i];
            char stem[128];
            const char* s1 = strrchr(jp, '/');
            const char* s2 = strrchr(jp, '\\');
            const char* b = (s1 > s2) ? s1 : s2;
            b = (b != NULL) ? b + 1 : jp;
            strncpy(stem, b, sizeof(stem) - 1);
            stem[sizeof(stem) - 1] = 0;
            if(strcmp(stem, "events.json") == 0 || strcmp(stem, "data.json") == 0)
                continue;
            if(EdLooksLikeChart(jp) && EdChart_LoadJSON(c, jp, dir, EdDirName(dir))) {
                UnloadDirectoryFiles(list);
                return 1;
            }
        }
        UnloadDirectoryFiles(list);
    }
    snprintf(p, sizeof(p), "%s/data.song", dir);
    if(EdChart_LoadBinary(c, p, dir, EdDirName(dir)))
        return 1;
    return 0;
}

// forward
static char EdChart_SaveEx(const EdChart* c, char compressed);

// ---------- salvar (json FNF) ----------
char EdChart_Save(const EdChart* c) {
    return EdChart_SaveEx(c, 0);
}

// ---------- salvar compacto (json minificado, campos padrão omitidos) ----------
char EdChart_SaveCompressed(const EdChart* c) {
    return EdChart_SaveEx(c, 1);
}

static char EdChart_SaveEx(const EdChart* c, char compressed) {
    if(c->dir[0] == 0)
        return 0;

    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val* js = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "song", js);

    yyjson_mut_obj_add_str(doc, js, "song", c->name);
    if(!compressed || c->bpm != 100.0f) yyjson_mut_obj_add_real(doc, js, "bpm", c->bpm);
    if(!compressed || c->speed != 1.0f) yyjson_mut_obj_add_real(doc, js, "speed", c->speed);
    yyjson_mut_obj_add_str(doc, js, "player1", c->p1);
    yyjson_mut_obj_add_str(doc, js, "player2", c->p2);
    yyjson_mut_obj_add_str(doc, js, "stage", c->stage);
    if(!compressed || (c->gf[0] != 0 && strcmp(c->gf, "gf") != 0))
        yyjson_mut_obj_add_str(doc, js, "gfVersion", c->gf[0] != 0 ? c->gf : "gf");
    if(!compressed || c->noteSkin[0] != 0)
        yyjson_mut_obj_add_str(doc, js, "noteSkin", c->noteSkin);
    if(c->noteSkin[0] == '@')   // devolve ao formato Psych para nao perder o arrowSkin
        yyjson_mut_obj_add_str(doc, js, "arrowSkin", c->noteSkin + 1);

    yyjson_mut_val* jnotes = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, js, "notes", jnotes);

    float runBpm = c->bpm;
    for(int i = 0; i < c->nsec; i++) {
        const EdSec* s = c->secs + i;
        yyjson_mut_val* jsec = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(jnotes, jsec);

        if(!compressed || s->lenSteps != 16)
            yyjson_mut_obj_add_int(doc, jsec, "lengthInSteps", s->lenSteps);
        if(!compressed || s->mustHit)
            yyjson_mut_obj_add_bool(doc, jsec, "mustHitSection", s->mustHit);

        char change = (s->bpm != runBpm);
        if(change) runBpm = s->bpm;
        if(!compressed || change)
            yyjson_mut_obj_add_real(doc, jsec, "bpm", s->bpm);
        if(!compressed || change)
            yyjson_mut_obj_add_bool(doc, jsec, "changeBPM", change);

        yyjson_mut_val* jarr = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, jsec, "sectionNotes", jarr);

        for(int k = 0; k < s->count; k++) {
            yyjson_mut_val* jn = yyjson_mut_arr(doc);
            yyjson_mut_arr_append(jarr, jn);
            yyjson_mut_arr_append(jn, yyjson_mut_real(doc, s->notes[k].time));
            yyjson_mut_arr_append(jn, yyjson_mut_int(doc, s->notes[k].lane));
            if(!compressed || s->notes[k].len != 0.0f)
                yyjson_mut_arr_append(jn, yyjson_mut_real(doc, s->notes[k].len));
        }
    }

    size_t len = 0;
    yyjson_write_flag flag = compressed ? 0 : YYJSON_WRITE_PRETTY;
    char* txt = yyjson_mut_write(doc, flag, &len);
    yyjson_mut_doc_free(doc);
    if(txt == NULL)
        return 0;

    char p[300];
    snprintf(p, sizeof(p), "%s/data.json", c->dir);
    FILE* f = fopen(p, "w");
    if(f == NULL) {
        free(txt);
        return 0;
    }
    fwrite(txt, 1, len, f);
    fclose(f);
    free(txt);
    return 1;
}

char EdCopyFile(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if(in == NULL) return 0;
    FILE* out = fopen(dst, "wb");
    if(out == NULL) {
        fclose(in);
        return 0;
    }
    char buf[8192];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 1;
}

void EdSanitizeName(char* dst, size_t dstSize, const char* src) {
    size_t w = 0;
    for(size_t i = 0; src[i] != 0 && w + 1 < dstSize; i++) {
        char ch = src[i];
        if((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
            dst[w++] = ch;
    }
    dst[w] = 0;
}

char statusText[128];

float statusTimer = 0;

void EdStatusMsg(const char* m) {
    strncpy(statusText, m, sizeof(statusText) - 1);
    statusText[sizeof(statusText) - 1] = 0;
    statusTimer = 3.0f;
}

// ---------- EDIT ----------
void EdTouch(void) {
    Undo_Push();
    chart.dirty = 1;
}
