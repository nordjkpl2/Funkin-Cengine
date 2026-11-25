/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: gravacao do chart em streaming.
 *
 * O JSON vai saindo direto pro arquivo por um buffer de 64 KB; nao ha arvore na memoria, entao o
 * custo de RAM e constante. Grava em data.json.tmp e so troca pelo data.json no fim: uma falha no
 * meio nao corrompe o chart anterior. */

#include "scenes/modding/charteditor/data/EditorChartSave.h"
#include "scenes/modding/charteditor/data/EditorEvents.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SAVE_BUF_CAP 65536

typedef struct {
    FILE* f;
    size_t n;
    char err;
    char pretty;
    char buf[SAVE_BUF_CAP];
} SaveOut;

static void Out_Flush(SaveOut* o) {
    if(o->n > 0 && !o->err && fwrite(o->buf, 1, o->n, o->f) != o->n)
        o->err = 1;
    o->n = 0;
}

static void Out_Bytes(SaveOut* o, const char* s, size_t len) {
    while(len > 0) {
        if(o->n == SAVE_BUF_CAP)
            Out_Flush(o);
        size_t k = SAVE_BUF_CAP - o->n;
        if(k > len) k = len;
        memcpy(o->buf + o->n, s, k);
        o->n += k;
        s += k;
        len -= k;
    }
}

static void Out_Str(SaveOut* o, const char* s) {
    Out_Bytes(o, s, strlen(s));
}

static void Out_Char(SaveOut* o, char ch) {
    if(o->n == SAVE_BUF_CAP)
        Out_Flush(o);
    o->buf[o->n++] = ch;
}

static void Out_Int(SaveOut* o, int v) {
    if(v >= 0 && v < 10) {
        Out_Char(o, (char)('0' + v));
        return;
    }
    char t[16];
    snprintf(t, sizeof(t), "%d", v);
    Out_Str(o, t);
}

/* numero real: %.9g volta exato pra float; sempre com ponto (1000 -> 1000.0) */
static void Out_Real(SaveOut* o, double v) {
    if(!isfinite(v))
        v = 0.0;
    char t[40];
    snprintf(t, sizeof(t), "%.9g", v);
    if(strpbrk(t, ".eE") == NULL)
        strncat(t, ".0", sizeof(t) - strlen(t) - 1);
    Out_Str(o, t);
}

static void Out_Bool(SaveOut* o, int v) {
    Out_Str(o, v ? "true" : "false");
}

static void Out_Quoted(SaveOut* o, const char* s) {
    Out_Char(o, '"');
    for(const unsigned char* p = (const unsigned char*)s; *p != 0; p++) {
        if(*p == '"' || *p == '\\') {
            Out_Char(o, '\\');
            Out_Char(o, (char)*p);
        } else if(*p < 0x20) {
            char t[8];
            snprintf(t, sizeof(t), "\\u%04x", (unsigned)*p);
            Out_Str(o, t);
        } else {
            Out_Char(o, (char)*p);
        }
    }
    Out_Char(o, '"');
}

/* quebra de linha + indentacao (so no modo pretty) */
static void Out_Nl(SaveOut* o, int level) {
    if(!o->pretty)
        return;
    Out_Char(o, '\n');
    for(int i = 0; i < level; i++)
        Out_Str(o, "    ");
}

/* separador antes de um item (virgula se nao for o primeiro) + quebra de linha */
static void Out_Item(SaveOut* o, int level, char* first) {
    if(!*first)
        Out_Char(o, ',');
    *first = 0;
    Out_Nl(o, level);
}

static void Out_Key(SaveOut* o, int level, char* first, const char* key) {
    Out_Item(o, level, first);
    Out_Quoted(o, key);
    Out_Char(o, ':');
    if(o->pretty)
        Out_Char(o, ' ');
}

static void WriteNote(SaveOut* o, const EdNote* nt, char compressed, char* firstNote) {
    Out_Item(o, 5, firstNote);
    Out_Char(o, '[');
    Out_Real(o, (double)nt->time);
    Out_Char(o, ',');
    if(o->pretty) Out_Char(o, ' ');
    Out_Int(o, nt->lane);
    if(!compressed || nt->len != 0.0f) {
        Out_Char(o, ',');
        if(o->pretty) Out_Char(o, ' ');
        Out_Real(o, (double)nt->len);
    }
    Out_Char(o, ']');
}

static void WriteSection(SaveOut* o, const EdSec* s, char compressed, float* runBpm, char* firstSec) {
    Out_Item(o, 3, firstSec);
    Out_Char(o, '{');
    char fF = 1;
    if(!compressed || s->lenSteps != 16) {
        Out_Key(o, 4, &fF, "lengthInSteps");
        Out_Int(o, s->lenSteps);
    }
    if(!compressed || s->mustHit) {
        Out_Key(o, 4, &fF, "mustHitSection");
        Out_Bool(o, s->mustHit);
    }
    char change = (s->bpm != *runBpm);
    if(change) *runBpm = s->bpm;
    if(!compressed || change) {
        Out_Key(o, 4, &fF, "bpm");
        Out_Real(o, s->bpm);
        Out_Key(o, 4, &fF, "changeBPM");
        Out_Bool(o, change);
    }

    Out_Key(o, 4, &fF, "sectionNotes");
    Out_Char(o, '[');
    char fNote = 1;
    for(int k = 0; k < s->count; k++)
        WriteNote(o, s->notes + k, compressed, &fNote);
    if(!fNote)
        Out_Nl(o, 4);
    Out_Char(o, ']');

    Out_Nl(o, 3);
    Out_Char(o, '}');
}

/* escreve os campos do objeto "song" (o cabecalho); *f controla a virgula entre os campos */
static void WriteHeader(SaveOut* o, const EdChart* c, char compressed, char* f) {
    Out_Key(o, 2, f, "song");
    Out_Quoted(o, c->name);
    if(!compressed || c->bpm != 100.0f) {
        Out_Key(o, 2, f, "bpm");
        Out_Real(o, c->bpm);
    }
    /* speed gravado com no maximo 1 casa decimal (1.0, 1.5...), sem o lixo do float */
    if(!compressed || c->speed != 1.0f) {
        Out_Key(o, 2, f, "speed");
        Out_Real(o, (double)(int)(c->speed * 10.0f + 0.5f) / 10.0);
    }
    Out_Key(o, 2, f, "player1");
    Out_Quoted(o, c->p1);
    Out_Key(o, 2, f, "player2");
    Out_Quoted(o, c->p2);
    Out_Key(o, 2, f, "stage");
    Out_Quoted(o, c->stage);
    if(!compressed || (c->gf[0] != 0 && strcmp(c->gf, "gf") != 0)) {
        Out_Key(o, 2, f, "gfVersion");
        Out_Quoted(o, c->gf[0] != 0 ? c->gf : "gf");
    }
    if(!compressed || c->noteSkin[0] != 0) {
        Out_Key(o, 2, f, "noteSkin");
        Out_Quoted(o, c->noteSkin);
    }
    if(c->noteSkin[0] == '@') { // devolve ao formato Psych para nao perder o arrowSkin
        Out_Key(o, 2, f, "arrowSkin");
        Out_Quoted(o, c->noteSkin + 1);
    }
}

static int WriteAll(SaveOut* o, const EdChart* c, char compressed) {
    Out_Char(o, '{');
    char first = 1;
    Out_Key(o, 1, &first, "song");
    Out_Char(o, '{');
    char fField = 1;
    WriteHeader(o, c, compressed, &fField);

    Out_Key(o, 2, &fField, "notes");
    Out_Char(o, '[');
    char firstSec = 1;
    float runBpm = c->bpm;
    for(int i = 0; i < c->nsec; i++)
        WriteSection(o, c->secs + i, compressed, &runBpm, &firstSec);
    if(!firstSec)
        Out_Nl(o, 2);
    Out_Char(o, ']');

    Out_Nl(o, 1);
    Out_Char(o, '}');
    Out_Nl(o, 0);
    Out_Char(o, '}');
    if(o->pretty)
        Out_Char(o, '\n');
    Out_Flush(o);
    return !o->err;
}

static char SaveEx(const EdChart* c, char compressed) {
    if(c->dir[0] == 0)
        return 0;

    char p[300], tmp[320];
    snprintf(p, sizeof(p), "%s/data.json", c->dir);
    snprintf(tmp, sizeof(tmp), "%s/data.json.tmp", c->dir);
    FILE* f = fopen(tmp, "wb");
    if(f == NULL)
        return 0;

    SaveOut* o = malloc(sizeof(SaveOut));
    if(o == NULL) {
        fclose(f);
        remove(tmp);
        return 0;
    }
    o->f = f;
    o->n = 0;
    o->err = 0;
    o->pretty = compressed ? 0 : 1;

    char ok = (char)WriteAll(o, c, compressed);
    if(fclose(f) != 0) ok = 0;
    free(o);
    if(!ok) {
        remove(tmp);
        return 0;
    }
    /* Windows nao troca por cima com rename: tira o antigo e renomeia (o novo ja esta inteiro em tmp) */
    remove(p);
    if(rename(tmp, p) != 0) {
        remove(tmp);
        return 0;
    }
    return EdEvents_Save(c->dir);
}

/* ---------- formato .song binario (inverso do Song_Parse) ---------- */

/* string: int32 tamanho (1..63) + bytes sem \0 */
static void Bin_Str(FILE* f, const char* s, const char* fallback) {
    if(s == NULL || s[0] == 0)
        s = fallback;
    int32_t len = (int32_t)strlen(s);
    if(len > SONG_NAME_MAX - 1)
        len = SONG_NAME_MAX - 1;
    fwrite(&len, sizeof(len), 1, f);
    fwrite(s, 1, (size_t)len, f);
}

/* lane (0-7) -> id do .song, invertendo a conta de Song_TryLoadBinary (EdChart_LoadBinary) */
static int16_t Bin_LaneToId(int lane, char mustHit) {
    if(mustHit)
        return (int16_t)(lane < 4 ? -(lane + 1) : lane - 3);
    return (int16_t)(lane < 4 ? lane + 1 : 3 - lane);
}

static char Bin_NoteOk(const EdNote* nt) {
    return nt->lane >= 0 && nt->lane <= 7 && nt->time >= 0.0f && nt->len >= 0.0f;
}

char EdChart_SaveBinary(const EdChart* c) {
    if(c->dir[0] == 0 || c->nsec < 1)
        return 0;

    char p[300], tmp[320];
    snprintf(p, sizeof(p), "%s/data.song", c->dir);
    snprintf(tmp, sizeof(tmp), "%s/data.song.tmp", c->dir);
    FILE* f = fopen(tmp, "wb");
    if(f == NULL)
        return 0;

    Bin_Str(f, c->p1, "bf");
    Bin_Str(f, c->p2, "dad");
    Bin_Str(f, c->stage, "stage");

    float speed = (c->speed > 0.0f) ? c->speed : 1.0f;
    int32_t ns = c->nsec;
    fwrite(&speed, sizeof(speed), 1, f);
    fwrite(&ns, sizeof(ns), 1, f);
    for(int i = 0; i < c->nsec; i++) {
        const EdSec* s = c->secs + i;
        int32_t len = (s->lenSteps >= 1) ? s->lenSteps : 16;
        float bpm = (s->bpm >= 10.0f) ? s->bpm : 100.0f;
        unsigned char mh = s->mustHit ? 1 : 0;
        int32_t n = 0;
        for(int k = 0; k < s->count; k++)
            n += Bin_NoteOk(s->notes + k);
        fwrite(&len, sizeof(len), 1, f);
        fwrite(&bpm, sizeof(bpm), 1, f);
        fwrite(&mh, 1, 1, f);
        fwrite(&n, sizeof(n), 1, f);
        for(int k = 0; k < s->count; k++) {
            const EdNote* nt = s->notes + k;
            if(!Bin_NoteOk(nt))
                continue;
            float time = nt->time;
            int16_t id = Bin_LaneToId(nt->lane, s->mustHit);
            int16_t pad = 0;
            float nlen = nt->len;
            fwrite(&time, sizeof(time), 1, f);
            fwrite(&id, sizeof(id), 1, f);
            fwrite(&pad, sizeof(pad), 1, f);
            fwrite(&nlen, sizeof(nlen), 1, f);
        }
    }

    char ok = (char)(ferror(f) == 0);
    if(fclose(f) != 0) ok = 0;
    if(!ok) {
        remove(tmp);
        return 0;
    }
    remove(p);
    if(rename(tmp, p) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

char EdChart_Save(const EdChart* c) {
    return SaveEx(c, 0);
}

char EdChart_SaveCompressed(const EdChart* c) {
    return SaveEx(c, 1);
}
