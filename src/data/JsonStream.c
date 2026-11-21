/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* JsonStream.c - leitor JSON por fluxo. Ver data/JsonStream.h. */

#include "data/JsonStream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JS_BLOCK 65536

struct JsonStream {
    FILE *f;
    size_t pos, len;
    int eof;
    int err;
    unsigned char buf[JS_BLOCK];
};

JsonStream *JsonStream_Open(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    JsonStream *js = calloc(1, sizeof(*js));
    if (js == NULL) {
        fclose(f);
        return NULL;
    }
    js->f = f;
    return js;
}

void JsonStream_Close(JsonStream *js) {
    if (js == NULL)
        return;
    fclose(js->f);
    free(js);
}

int JsonStream_Failed(const JsonStream *js) { return js->err; }
void JsonStream_Fail(JsonStream *js) { js->err = 1; }

static int Refill(JsonStream *js) {
    if (js->pos < js->len)
        return 1;
    if (js->eof)
        return 0;
    js->len = fread(js->buf, 1, JS_BLOCK, js->f);
    js->pos = 0;
    if (js->len == 0) {
        js->eof = 1;
        return 0;
    }
    return 1;
}

static int RawPeek(JsonStream *js) {
    if (!Refill(js))
        return -1;
    return js->buf[js->pos];
}

static int RawNext(JsonStream *js) {
    int c = RawPeek(js);
    if (c >= 0)
        js->pos++;
    return c;
}

static int IsWs(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

int JsonStream_Peek(JsonStream *js) {
    if (js->err)
        return -1;
    for (;;) {
        int c = RawPeek(js);
        if (c < 0 || !IsWs(c))
            return c;
        js->pos++;
    }
}

int JsonStream_TryEat(JsonStream *js, int ch) {
    if (JsonStream_Peek(js) != ch)
        return 0;
    js->pos++;
    return 1;
}

int JsonStream_Eat(JsonStream *js, int ch) {
    if (JsonStream_TryEat(js, ch))
        return 1;
    js->err = 1;
    return 0;
}

/* corpo de string com a aspa de abertura ja consumida */
static int ReadStringBody(JsonStream *js, char *out, size_t cap) {
    size_t w = 0;
    for (;;) {
        int c = RawNext(js);
        if (c < 0) {
            js->err = 1;
            return 0;
        }
        if (c == '"')
            break;
        int ch = c;
        if (c == '\\') {
            int e = RawNext(js);
            if (e < 0) {
                js->err = 1;
                return 0;
            }
            switch (e) {
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case '"': case '\\': case '/': ch = e; break;
            case 'u': {
                unsigned code = 0;
                for (int i = 0; i < 4; i++) {
                    int h = RawNext(js);
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
                    else {
                        js->err = 1;
                        return 0;
                    }
                }
                ch = code < 0x80 ? (int)code : '?';
                break;
            }
            default:
                js->err = 1;
                return 0;
            }
        }
        if (out != NULL && w + 1 < cap)
            out[w++] = (char)ch;
    }
    if (out != NULL && cap > 0)
        out[w] = '\0';
    return 1;
}

int JsonStream_ReadString(JsonStream *js, char *out, size_t cap) {
    if (JsonStream_Peek(js) != '"')
        return 0;
    js->pos++;
    return ReadStringBody(js, out, cap);
}

int JsonStream_ReadNumber(JsonStream *js, double *out) {
    int c = JsonStream_Peek(js);
    if (c != '-' && (c < '0' || c > '9'))
        return 0;
    char tok[64];
    size_t n = 0;
    while (c >= 0 && ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
        if (n < sizeof(tok) - 1)
            tok[n++] = (char)c;
        js->pos++;
        c = RawPeek(js);
    }
    tok[n] = '\0';
    *out = strtod(tok, NULL);
    return 1;
}

int JsonStream_ReadBool(JsonStream *js, int *out) {
    int c = JsonStream_Peek(js);
    if (c != 't' && c != 'f')
        return 0;
    char tok[8];
    size_t n = 0;
    while ((c = RawPeek(js)) >= 'a' && c <= 'z') {
        if (n < sizeof(tok) - 1)
            tok[n++] = (char)c;
        js->pos++;
    }
    tok[n] = '\0';
    if (strcmp(tok, "true") == 0) {
        *out = 1;
        return 1;
    }
    if (strcmp(tok, "false") == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

void JsonStream_SkipValue(JsonStream *js) {
    int c = JsonStream_Peek(js);
    if (c < 0) {
        js->err = 1;
        return;
    }
    if (c == '"') {
        js->pos++;
        ReadStringBody(js, NULL, 0);
        return;
    }
    if (c == '{' || c == '[') {
        int depth = 0;
        do {
            c = RawNext(js);
            if (c < 0) {
                js->err = 1;
                return;
            }
            if (c == '"') {
                if (!ReadStringBody(js, NULL, 0))
                    return;
            } else if (c == '{' || c == '[') {
                depth++;
            } else if (c == '}' || c == ']') {
                depth--;
            }
        } while (depth > 0);
        return;
    }
    /* escalar (numero, true, false, null): consome ate um delimitador e valida o token */
    char tok[32];
    size_t n = 0;
    while ((c = RawPeek(js)) >= 0 && !IsWs(c) && c != ',' && c != ']' && c != '}' && c != ':') {
        if (n < sizeof(tok) - 1)
            tok[n++] = (char)c;
        js->pos++;
    }
    tok[n] = '\0';
    if (n == 0) {
        js->err = 1;
        return;
    }
    if (strcmp(tok, "true") == 0 || strcmp(tok, "false") == 0 || strcmp(tok, "null") == 0)
        return;
    if (n == sizeof(tok) - 1) { /* token longo demais para checar por inteiro: so o primeiro byte */
        if (tok[0] != '-' && (tok[0] < '0' || tok[0] > '9'))
            js->err = 1;
        return;
    }
    char *end = NULL;
    strtod(tok, &end);
    if (end == tok || *end != '\0')
        js->err = 1;
}
