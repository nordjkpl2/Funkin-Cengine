/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: carregamento do chart.
 *
 * O JSON e lido em streaming (data/JsonStream.h): cada secao e montada como um trecho do editor
 * e a memoria cresce so com o chart final (as notas). Nao ha limite de tamanho do arquivo.
 *
 * Ordem: o cabecalho (bpm, speed, nomes) pode vir antes ou depois do array de notas no arquivo.
 * Por isso os metadados de cada secao (bpm, changeBPM, steps, turno) ficam guardados e so sao
 * aplicados no fim, quando o bpm inicial ja e conhecido. */

#include "scenes/modding/charteditor/data/EditorChartLoad.h"
#include "scenes/modding/charteditor/data/EditorChartModel.h"
#include "scenes/modding/charteditor/data/EditorEvents.h"
#include "scenes/modding/charteditor/data/EditorFiles.h"
#include "scenes/modding/charteditor/data/EditorSection.h"
#include "data/JsonStream.h"
#include "data/Song.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* metadados de uma secao, lidos do JSON e aplicados em Finish */
typedef struct {
    double lenA, lenB, bpm; /* lengthInSteps, lengthinsteps, bpm */
    char hasLenA, hasLenB, hasBpm, mustHit, changeBPM;
} SecMeta;

typedef struct LoadState LoadState;
typedef void (*MemberFn)(LoadState* st, const char* key);

struct LoadState {
    JsonStream* js;
    const char* dir;
    const char* fallbackName;
    char probe;     /* so verifica se ha notas (para o cata-json de OpenDir) */
    char found;     /* probe: achou notas */
    char songSeen;  /* objeto "song" ja lido: o resto da raiz e ignorado */
    char notesSeen;
    char hasArrow;
    char arrow[SONG_NAME_MAX]; /* arrowSkin (Psych), usado se noteSkin vier vazio */
    EdChart c;      /* cabecalho; secs so e' preenchido no fim */
    EdSec* secs;    /* secoes lidas, em ordem */
    SecMeta* meta;  /* paralelo a secs */
    int nsec, capSecs;
    EdSec* curSec;      /* secao em leitura */
    SecMeta* curMeta;
};

static int ToInt(double v) {
    if(!(v > -1e9)) return -1000000000; /* inclui NaN */
    if(v > 1e9) return 1000000000;
    return (int)v;
}

static int ReadStr(JsonStream* js, char* dst, size_t cap) {
    if(JsonStream_ReadString(js, dst, cap)) return 1;
    JsonStream_SkipValue(js);
    return 0;
}

static int ReadNum(JsonStream* js, double* out) {
    if(JsonStream_ReadNumber(js, out)) return 1;
    JsonStream_SkipValue(js);
    return 0;
}

static int ReadBoolVal(JsonStream* js, int* out) {
    if(JsonStream_ReadBool(js, out)) return 1;
    JsonStream_SkipValue(js);
    return 0;
}

/* objeto: { "k": valor, ... } com fn lendo cada valor. Consome o valor inteiro, mesmo sem usar. */
static void ParseObject(LoadState* st, MemberFn fn) {
    JsonStream* js = st->js;
    if(!JsonStream_Eat(js, '{')) return;
    if(JsonStream_TryEat(js, '}')) return;
    do {
        char key[40];
        if(!JsonStream_ReadString(js, key, sizeof(key))) {
            JsonStream_Fail(js);
            return;
        }
        if(!JsonStream_Eat(js, ':')) return;
        fn(st, key);
        if(JsonStream_Failed(js)) return;
    } while(JsonStream_TryEat(js, ','));
    JsonStream_Eat(js, '}');
}

static void ClearSecs(LoadState* st) {
    for(int i = 0; i < st->nsec; i++)
        EdSec_Free(st->secs + i);
    free(st->secs);
    free(st->meta);
    st->secs = NULL;
    st->meta = NULL;
    st->nsec = 0;
    st->capSecs = 0;
}

static void ResetHeader(LoadState* st) {
    ClearSecs(st);
    EdChart_Free(&st->c);
    EdChart_Default(&st->c, st->fallbackName);
    strncpy(st->c.dir, st->dir, sizeof(st->c.dir) - 1);
    st->c.dir[sizeof(st->c.dir) - 1] = 0;
    st->hasArrow = 0;
    st->notesSeen = 0;
}

/* ---------- notas ---------- */

/* uma nota: [tempo, casa, (comprimento)]. Precisa de pelo menos 2 valores; o resto e ignorado. */
static void ParseNote(LoadState* st) {
    JsonStream* js = st->js;
    double v[3] = {0, 0, 0};
    char isNum[3] = {0, 0, 0};
    int n = 0;
    if(!JsonStream_Eat(js, '[')) return;
    if(!JsonStream_TryEat(js, ']')) {
        do {
            if(n < 3 && JsonStream_ReadNumber(js, &v[n])) isNum[n] = 1;
            else JsonStream_SkipValue(js);
            n++;
        } while(JsonStream_TryEat(js, ','));
        JsonStream_Eat(js, ']');
    }
    if(JsonStream_Failed(js) || n < 2)
        return;
    float time = isNum[0] ? (float)v[0] : 0.0f;
    int lane = (isNum[1] ? ToInt(v[1]) : 0) % 8;
    float len = (isNum[2] && v[2] > 0) ? (float)v[2] : 0.0f;
    EdSec_AddBulk(st->curSec, time, lane, len);
}

static void ParseSectionNotes(LoadState* st) {
    JsonStream* js = st->js;
    if(!JsonStream_Eat(js, '[')) return;
    if(JsonStream_TryEat(js, ']')) return;
    do {
        if(JsonStream_Peek(js) == '[') ParseNote(st);
        else JsonStream_SkipValue(js);
    } while(JsonStream_TryEat(js, ','));
    JsonStream_Eat(js, ']');
}

static void SectionMember(LoadState* st, const char* key) {
    JsonStream* js = st->js;
    SecMeta* m = st->curMeta;
    if(strcmp(key, "lengthInSteps") == 0) {
        if(ReadNum(js, &m->lenA)) m->hasLenA = 1;
    } else if(strcmp(key, "lengthinsteps") == 0) {
        if(ReadNum(js, &m->lenB)) m->hasLenB = 1;
    } else if(strcmp(key, "mustHitSection") == 0) {
        int v = 0;
        if(ReadBoolVal(js, &v)) m->mustHit = (char)v;
    } else if(strcmp(key, "bpm") == 0) {
        if(ReadNum(js, &m->bpm)) m->hasBpm = 1;
    } else if(strcmp(key, "changeBPM") == 0) {
        int v = 0;
        if(ReadBoolVal(js, &v)) m->changeBPM = (char)v;
    } else if(strcmp(key, "sectionNotes") == 0) {
        if(JsonStream_Peek(js) == '[') ParseSectionNotes(st);
        else JsonStream_SkipValue(js);
    } else {
        JsonStream_SkipValue(js);
    }
}

static int PushSection(LoadState* st, EdSec* s, SecMeta* m) {
    if(st->nsec >= st->capSecs) {
        int nc = st->capSecs ? st->capSecs * 2 : 64;
        EdSec* g = realloc(st->secs, sizeof(EdSec) * (size_t)nc);
        if(g == NULL) return 0;
        st->secs = g;
        SecMeta* gm = realloc(st->meta, sizeof(SecMeta) * (size_t)nc);
        if(gm == NULL) return 0;
        st->meta = gm;
        st->capSecs = nc;
    }
    st->secs[st->nsec] = *s;
    st->meta[st->nsec] = *m;
    st->nsec++;
    return 1;
}

/* array "notes": cada elemento vira um trecho */
static void ParseNotes(LoadState* st) {
    JsonStream* js = st->js;
    if(!JsonStream_Eat(js, '[')) return;
    if(st->probe) {
        /* so queremos saber se ha alguma nota: nao precisa ler o resto */
        if(JsonStream_Peek(js) != ']') st->found = 1;
        JsonStream_Fail(js);
        return;
    }
    st->notesSeen = 1;
    EdChart_Free(&st->c); /* descarta o trecho padrao: as secoes vem do arquivo */
    if(JsonStream_TryEat(js, ']')) return;
    do {
        EdSec s;
        SecMeta m;
        EdSec_Default(&s, 0.0f);
        memset(&m, 0, sizeof(m));
        if(JsonStream_Peek(js) == '{') {
            st->curSec = &s;
            st->curMeta = &m;
            ParseObject(st, SectionMember);
            st->curSec = NULL;
            st->curMeta = NULL;
            EdSec_FinishBulk(&s);
        } else {
            JsonStream_SkipValue(js); /* elemento invalido: vira trecho padrao, como antes */
        }
        if(JsonStream_Failed(js) || !PushSection(st, &s, &m)) {
            EdSec_Free(&s);
            JsonStream_Fail(js);
            return;
        }
    } while(JsonStream_TryEat(js, ','));
    JsonStream_Eat(js, ']');
}

/* ---------- raiz e cabecalho ---------- */

static void SongMember(LoadState* st, const char* key) {
    JsonStream* js = st->js;
    EdChart* c = &st->c;
    double v = 0;
    if(strcmp(key, "notes") == 0) {
        if(JsonStream_Peek(js) == '[') ParseNotes(st);
        else JsonStream_SkipValue(js);
    } else if(strcmp(key, "song") == 0) {
        ReadStr(js, c->name, sizeof(c->name));
    } else if(strcmp(key, "player1") == 0) {
        ReadStr(js, c->p1, sizeof(c->p1));
    } else if(strcmp(key, "player2") == 0) {
        ReadStr(js, c->p2, sizeof(c->p2));
    } else if(strcmp(key, "stage") == 0) {
        ReadStr(js, c->stage, sizeof(c->stage));
    } else if(strcmp(key, "gfVersion") == 0) {
        ReadStr(js, c->gf, sizeof(c->gf));
    } else if(strcmp(key, "noteSkin") == 0) {
        if(!ReadStr(js, c->noteSkin, sizeof(c->noteSkin))) c->noteSkin[0] = 0;
    } else if(strcmp(key, "arrowSkin") == 0) {
        st->hasArrow = (char)ReadStr(js, st->arrow, sizeof(st->arrow));
    } else if(strcmp(key, "bpm") == 0) {
        if(ReadNum(js, &v) && v >= 10) c->bpm = (float)v;
    } else if(strcmp(key, "speed") == 0) {
        if(ReadNum(js, &v) && v > 0) c->speed = (float)v;
    } else {
        JsonStream_SkipValue(js);
    }
}

static void RootMember(LoadState* st, const char* key) {
    JsonStream* js = st->js;
    if(strcmp(key, "song") == 0 && JsonStream_Peek(js) == '{' && !st->songSeen) {
        /* "song" e o chart: descarta o que a raiz tiver trazido antes (como o formato antigo fazia) */
        ResetHeader(st);
        st->songSeen = 1;
        ParseObject(st, SongMember);
    } else if(!st->songSeen) {
        SongMember(st, key); /* formato sem o objeto "song": a propria raiz e o chart */
    } else {
        JsonStream_SkipValue(js);
    }
}

static int RunLoad(LoadState* st) {
    if(JsonStream_Peek(st->js) != '{') return 0;
    ParseObject(st, RootMember);
    if(st->probe) return st->found;
    if(JsonStream_Failed(st->js) || !st->notesSeen) return 0;
    return 1;
}

/* Aplica os metadados das secoes (bpm corrente, steps, turno) e entrega as secoes ao cabecalho. */
static void Finish(LoadState* st) {
    EdChart* c = &st->c;
    float runBpm = c->bpm;
    for(int i = 0; i < st->nsec; i++) {
        EdSec* s = st->secs + i;
        const SecMeta* m = st->meta + i;
        if(m->hasLenA && m->lenA >= 1) s->lenSteps = ToInt(m->lenA);
        else if(!m->hasLenA && m->hasLenB && m->lenB >= 1) s->lenSteps = ToInt(m->lenB);
        else s->lenSteps = 16;
        s->mustHit = m->mustHit;
        s->changeBPM = 0;
        if(m->changeBPM && m->hasBpm && m->bpm >= 10.0) {
            runBpm = (float)m->bpm;
            s->changeBPM = 1;
        }
        s->bpm = runBpm;
    }

    if(st->nsec > 0) {
        c->secs = st->secs;
        c->nsec = st->nsec;
        st->secs = NULL; /* posse passa ao chart */
        st->nsec = 0;
    } else {
        /* "notes": [] -> um trecho padrao, igual ao chart novo */
        free(st->secs);
        st->secs = NULL;
        c->secs = calloc(1, sizeof(EdSec));
        if(c->secs != NULL) {
            EdSec_Default(c->secs, 100.0f);
            c->nsec = 1;
        }
    }

    if(c->noteSkin[0] == 0 && st->hasArrow && st->arrow[0] != 0 &&
       strcmp(st->arrow, "NOTE_assets") != 0 && strlen(st->arrow) + 1 < sizeof(c->noteSkin))
        snprintf(c->noteSkin, sizeof(c->noteSkin), "@%s", st->arrow);
}

static void LoadState_Init(LoadState* st, JsonStream* js, const char* dir, const char* fallbackName, char probe) {
    memset(st, 0, sizeof(*st));
    st->js = js;
    st->dir = dir;
    st->fallbackName = fallbackName;
    st->probe = probe;
    EdChart_Default(&st->c, fallbackName);
    strncpy(st->c.dir, dir, sizeof(st->c.dir) - 1);
    st->c.dir[sizeof(st->c.dir) - 1] = 0;
}

static void LoadState_Free(LoadState* st) {
    ClearSecs(st);
    EdChart_Free(&st->c);
}

static char EdLooksLikeChart(const char* path, const char* dir) {
    JsonStream* js = JsonStream_Open(path);
    if(js == NULL) return 0;
    LoadState st;
    LoadState_Init(&st, js, dir, "", 1);
    RunLoad(&st);
    char ok = (char)st.found;
    LoadState_Free(&st);
    JsonStream_Close(js);
    return ok;
}

char EdChart_LoadJSON(EdChart* c, const char* jsonPath, const char* dir, const char* fallbackName) {
    JsonStream* js = JsonStream_Open(jsonPath);
    if(js == NULL) return 0;
    LoadState st;
    LoadState_Init(&st, js, dir, fallbackName, 0);
    char ok = (char)RunLoad(&st);
    JsonStream_Close(js);
    if(!ok) {
        LoadState_Free(&st);
        return 0;
    }
    Finish(&st);
    free(st.meta);
    EdChart_Free(c);
    *c = st.c;
    return 1;
}

/* ---------- formato .song legado (binario) ---------- */

static char EdChart_LoadBinary(EdChart* c, const char* binPath, const char* dir, const char* fallbackName) {
    FILE* t = fopen(binPath, "rb");
    if(t == NULL)
        return 0;
    fclose(t);
    Song s;
    memset(&s, 0, sizeof(s));
    Song_Parse(&s, binPath); /* sai do jogo se o arquivo for invalido; checado acima */

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
            EdSec_Default(dst, src->bpm);
            dst->mustHit = src->mustHit;
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

/* ---------- pasta do chart ---------- */

char EdChart_OpenDir(EdChart* c, const char* dir) {
    char p[300];
    EdEvents_Load(dir);
    snprintf(p, sizeof(p), "%s/data.json", dir);
    if(EdChart_LoadJSON(c, p, dir, EdDirName(dir)))
        return 1;
    // Psych: <pasta>.json (milf/milf.json)
    snprintf(p, sizeof(p), "%s/%s.json", dir, EdDirName(dir));
    if(EdLooksLikeChart(p, dir) && EdChart_LoadJSON(c, p, dir, EdDirName(dir)))
        return 1;
    // qualquer outro *.json que tenha notas (satin-panties-*.json, etc)
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
            if(EdLooksLikeChart(jp, dir) && EdChart_LoadJSON(c, jp, dir, EdDirName(dir))) {
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
