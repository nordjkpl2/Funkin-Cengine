/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongStream.c - chart JSON em modo streaming ("janelas por secao"). Ver data/SongStream.h.
 *
 * Abertura: o arquivo e lido uma vez e varrido byte a byte para achar o trecho de cada secao
 * (objeto JSON) dentro de song.notes. Cada trecho e parseado sozinho (um DOM pequeno, descartado
 * logo em seguida) para pegar meta, o inicio no tempo e o maior fim de nota. O cabecalho
 * (nomes, bpm, speed) vem de uma copia do objeto "song" em que o array de notas virou "[]".
 *
 * Durante a musica: SongStream_LoadSection le so o trecho pedido do arquivo (fseek/fread) e
 * converte com a mesma funcao do carregamento completo. Assim o resultado e identico.
 */

#include "data/song/Song.h"
#include "data/song/SongInternal.h"
#include "data/song/SongStream.h"
#include "yyjson.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONG_STREAM_INIT_SECTIONS 256

typedef struct SongStreamSection {
    size_t offset;      /* inicio do objeto da secao no arquivo */
    size_t length;      /* bytes do objeto */
    float loadMs;       /* carregar a secao ate este tempo (ms): menor entre o inicio e a primeira nota,
                           com sufixo-minimo (uma secao so e pulada quando todas as seguintes tambem sao) */
    float maxEndMs;     /* maior (tempo + comprimento) das notas; -FLT_MAX se a secao esta vazia */
    char mustHit;
} SongStreamSection;

struct SongStream {
    FILE *file;                 /* aberto durante a musica: as secoes sao lidas sob demanda */
    SongStreamSection *sections;
    int count;
};

/* ------------------------------------------------------------------------- */
/* Varredura do texto JSON (so limites de valores; quem interpreta e o yyjson) */
/* ------------------------------------------------------------------------- */

static const char *SS_Ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    return p;
}

/* p aponta para a aspa de abertura. Retorna o ponteiro logo apos a aspa de fechamento, ou NULL. */
static const char *SS_SkipString(const char *p, const char *end)
{
    if (p >= end || *p != '"') return NULL;
    for (++p; p < end; ++p) {
        if (*p == '\\') {
            if (p + 1 >= end) return NULL;
            ++p;
        } else if (*p == '"') {
            return p + 1;
        }
    }
    return NULL;
}

/* p aponta para o primeiro caractere de um valor. Retorna o ponteiro logo apos o valor, ou NULL. */
static const char *SS_SkipValue(const char *p, const char *end)
{
    int depth = 0;
    if (p >= end) return NULL;
    if (*p == '"') return SS_SkipString(p, end);
    if (*p != '{' && *p != '[') {
        while (p < end && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        return p;
    }
    while (p < end) {
        if (*p == '"') {
            p = SS_SkipString(p, end);
            if (p == NULL) return NULL;
            continue;
        }
        if (*p == '{' || *p == '[') {
            ++depth;
        } else if (*p == '}' || *p == ']') {
            --depth;
            if (depth == 0) return p + 1;
        }
        ++p;
    }
    return NULL;
}

/* obj aponta para um objeto JSON. Acha a chave `key` (sem escapes) e devolve o inicio do valor;
 * *valueEnd recebe o fim. NULL se nao achar ou se o texto nao for o esperado. */
static const char *SS_FindMember(const char *obj, const char *end, const char *key, const char **valueEnd)
{
    const size_t keyLen = strlen(key);
    const char *p = SS_Ws(obj, end);

    if (p >= end || *p != '{') return NULL;
    p = SS_Ws(p + 1, end);
    while (p < end && *p != '}') {
        const char *keyClose;
        const char *value;
        const char *after;
        size_t len;
        int same;

        if (*p == ',') {
            p = SS_Ws(p + 1, end);
            continue;
        }
        if (*p != '"') return NULL;
        keyClose = SS_SkipString(p, end);
        if (keyClose == NULL) return NULL;
        len = (size_t)(keyClose - p) - 2u;          /* sem as aspas */
        same = len == keyLen && memcmp(p + 1, key, keyLen) == 0;

        p = SS_Ws(keyClose, end);
        if (p >= end || *p != ':') return NULL;
        value = SS_Ws(p + 1, end);
        after = SS_SkipValue(value, end);
        if (after == NULL || after == value) return NULL;
        if (same) {
            *valueEnd = after;
            return value;
        }
        p = SS_Ws(after, end);
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Abertura                                                                  */
/* ------------------------------------------------------------------------- */

static int SS_PushSection(SongStreamSection **list, int *count, int *cap, size_t offset, size_t length)
{
    if (*count >= SONG_MAX_SECTIONS) return 0;
    if (*count == *cap) {
        int newCap = *cap > 0 ? *cap * 2 : SONG_STREAM_INIT_SECTIONS;
        SongStreamSection *grown;
        if (newCap > SONG_MAX_SECTIONS) newCap = SONG_MAX_SECTIONS;
        grown = (SongStreamSection *)realloc(*list, sizeof(**list) * (size_t)newCap);
        if (grown == NULL) return 0;
        *list = grown;
        *cap = newCap;
    }
    (*list)[*count].offset = offset;
    (*list)[*count].length = length;
    (*list)[*count].loadMs = 0.0f;
    (*list)[*count].maxEndMs = -FLT_MAX;
    (*list)[*count].mustHit = 0;
    ++*count;
    return 1;
}

/* Cabecalho: copia o objeto "song" trocando o array de notas por [] e le so o cabecalho dele. */
static int SS_ReadHeader(const char *songStart, const char *songEnd,
                         const char *notesStart, const char *notesEnd, Song *out)
{
    const size_t prefix = (size_t)(notesStart - songStart);
    const size_t suffix = (size_t)(songEnd - notesEnd);
    const size_t total = prefix + 2u + suffix;
    char *header = (char *)malloc(total + 1u);
    yyjson_doc *doc;
    int ok = 0;

    if (header == NULL) return 0;
    memcpy(header, songStart, prefix);
    memcpy(header + prefix, "[]", 2u);
    memcpy(header + prefix + 2u, notesEnd, suffix);
    header[total] = '\0';

    doc = yyjson_read(header, total, 0);
    free(header);
    if (doc == NULL) return 0;
    if (yyjson_is_obj(yyjson_doc_get_root(doc))) {
        Song_ReadHeader(yyjson_doc_get_root(doc), out);
        ok = 1;
    }
    yyjson_doc_free(doc);
    return ok;
}

SongStream *SongStream_Open(const char *jsonPath, Song *song)
{
    char *buffer = NULL;
    size_t size = 0;
    const char *end;
    const char *rootStart;
    const char *rootEnd;
    const char *songStart;
    const char *songEnd;
    const char *notesStart;
    const char *notesEnd;
    const char *p;
    const char *limit;
    SongStreamSection *list = NULL;
    int count = 0;
    int cap = 0;
    Section *meta = NULL;
    Song parsed;
    float running;
    float startMs = 0.0f;
    float prevMaxMs = -FLT_MAX;
    SongStream *stream = NULL;
    int ok = 0;

    if (jsonPath == NULL || song == NULL) return NULL;
    if (!Song_ReadFile(jsonPath, &buffer, &size)) return NULL;
    end = buffer + size;

    /* Raiz e objeto "song" (ou a propria raiz). */
    rootStart = SS_Ws(buffer, end);
    if (rootStart >= end || *rootStart != '{') goto cleanup;
    rootEnd = SS_SkipValue(rootStart, end);
    if (rootEnd == NULL) goto cleanup;

    {
        const char *songValueEnd = NULL;
        const char *songValue = SS_FindMember(rootStart, rootEnd, "song", &songValueEnd);
        if (songValue != NULL && *songValue == '{') {
            songStart = songValue;
            songEnd = songValueEnd;
        } else {
            songStart = rootStart;
            songEnd = rootEnd;
        }
    }

    notesStart = SS_FindMember(songStart, songEnd, "notes", &notesEnd);
    if (notesStart == NULL || *notesStart != '[') goto cleanup;

    memset(&parsed, 0, sizeof(parsed));
    if (!SS_ReadHeader(songStart, songEnd, notesStart, notesEnd, &parsed)) goto cleanup;

    /* Trechos das secoes: um objeto por elemento do array (mesma contagem do carregamento completo). */
    limit = notesEnd - 1;   /* o ']' */
    p = notesStart + 1;
    for (;;) {
        const char *elemEnd;
        p = SS_Ws(p, limit);
        if (p >= limit) break;
        if (*p == ',') {
            ++p;
            continue;
        }
        elemEnd = SS_SkipValue(p, limit);
        if (elemEnd == NULL || elemEnd <= p || elemEnd > limit) goto cleanup;
        if (!SS_PushSection(&list, &count, &cap, (size_t)(p - buffer), (size_t)(elemEnd - p))) goto cleanup;
        p = elemEnd;
    }
    if (count == 0) goto cleanup;

    /* Meta, inicio no tempo e faixa de notas de cada secao, em ordem. Uma secao por vez. */
    meta = (Section *)calloc((size_t)count, sizeof(Section));
    if (meta == NULL) goto cleanup;

    running = parsed.bpm;
    for (int k = 0; k < count; ++k) {
        SongStreamSection *sec = &list[k];
        yyjson_doc *doc = yyjson_read(buffer + sec->offset, sec->length, 0);
        yyjson_val *root = doc != NULL ? yyjson_doc_get_root(doc) : NULL;
        Section *section = &meta[k];

        if (root != NULL && yyjson_is_obj(root)) {
            Song_ReadSectionMeta(root, &running, section);
        } else {
            section->len = 16;
            section->bpm = running;
        }
        sec->mustHit = section->mustHit;
        sec->loadMs = startMs;

        if (root != NULL && yyjson_is_obj(root)) {
            DataNote *notes = NULL;
            int noteCount = 0;
            if (Song_ConvertSectionNotes(root,
                                         section->mustHit, &notes, &noteCount)) {
                /* Streaming assume secoes em ordem de tempo: notas de uma secao nao podem
                   voltar antes do fim da anterior. Senao, cai no carregamento completo. */
                if (notes[0].time < prevMaxMs) {
                    free(notes);
                    if (doc != NULL) yyjson_doc_free(doc);
                    goto cleanup;
                }
                for (int j = 0; j < noteCount; ++j) {
                    const float end2 = notes[j].time + notes[j].len;
                    if (end2 > sec->maxEndMs) sec->maxEndMs = end2;
                }
                if (notes[0].time < sec->loadMs) sec->loadMs = notes[0].time;
                prevMaxMs = notes[noteCount - 1].time;
                free(notes);
            }
        }
        if (doc != NULL) yyjson_doc_free(doc);

        /* Duracao da secao: len passos de (60000 / bpm / 4) ms. */
        startMs += (float)section->len * 60000.0f / (section->bpm * 4.0f);
    }

    /* Sufixo-minimo: as secoes carregam em ordem, entao o gatilho de k vale o menor dos seguintes. */
    for (int k = count - 2; k >= 0; --k) {
        if (list[k + 1].loadMs < list[k].loadMs) list[k].loadMs = list[k + 1].loadMs;
    }

    stream = (SongStream *)calloc(1, sizeof(*stream));
    if (stream == NULL) goto cleanup;
    stream->file = fopen(jsonPath, "rb");
    if (stream->file == NULL) goto cleanup;
    stream->sections = list;
    stream->count = count;
    list = NULL;   /* o stream fica com a lista */

    parsed.sections = meta;
    parsed.sectionCount = count;
    meta = NULL;
    *song = parsed;
    memset(&parsed, 0, sizeof(parsed));
    ok = 1;

cleanup:
    free(buffer);
    if (!ok) {
        if (stream != NULL) {
            if (stream->file != NULL) fclose(stream->file);
            free(stream->sections);
            free(stream);
        }
        free(list);
        free(meta);
        return NULL;
    }
    free(meta);
    free(list);
    return stream;
}

void SongStream_Close(SongStream *stream)
{
    if (stream == NULL) return;
    if (stream->file != NULL) fclose(stream->file);
    free(stream->sections);
    free(stream);
}

int SongStream_SectionCount(const SongStream *stream)
{
    return stream != NULL ? stream->count : 0;
}

float SongStream_SectionLoadMs(const SongStream *stream, int k)
{
    if (stream == NULL || k < 0 || k >= stream->count) return FLT_MAX;
    return stream->sections[k].loadMs;
}

float SongStream_SectionMaxEndMs(const SongStream *stream, int k)
{
    if (stream == NULL || k < 0 || k >= stream->count) return -FLT_MAX;
    return stream->sections[k].maxEndMs;
}

int SongStream_LoadSection(const SongStream *stream, int k, DataNote **out, char *mustHit)
{
    const SongStreamSection *sec;
    char *buf;
    yyjson_doc *doc;
    yyjson_val *root;
    int count = 0;

    if (out != NULL) *out = NULL;
    if (mustHit != NULL) *mustHit = 0;
    if (stream == NULL || out == NULL || k < 0 || k >= stream->count) return 0;

    sec = &stream->sections[k];
    if (mustHit != NULL) *mustHit = sec->mustHit;
    if (sec->length == 0 || stream->file == NULL) return 0;

    buf = (char *)malloc(sec->length);
    if (buf == NULL) return 0;
    if (fseek(stream->file, (long)sec->offset, SEEK_SET) != 0 ||
        fread(buf, 1, sec->length, stream->file) != sec->length) {
        free(buf);
        return 0;
    }
    doc = yyjson_read(buf, sec->length, 0);
    free(buf);
    if (doc == NULL) return 0;

    root = yyjson_doc_get_root(doc);
    if (root != NULL && yyjson_is_obj(root)) {
        (void)Song_ConvertSectionNotes(root,
                                       sec->mustHit, out, &count);
    }
    yyjson_doc_free(doc);
    return count;
}
