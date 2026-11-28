/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * StageLuaParse.c - Parser de subset de stage .lua (Psych) para sprites. Sem VM Lua.
 *
 * Le o texto uma vez, acompanhando os blocos (function/if/do/end) para saber em qual
 * callback cada chamada esta:
 *   topo, onCreate, onCreatePost, onSongStart, funcoes auxiliares -> monta o stage
 *   onBeatHit, onStepHit, onSectionHit, onCountdownTick          -> playAnim com condicao
 *   outras callbacks da Psych (onUpdate, onEvent...)              -> ignoradas
 * Variaveis simples (local x = 10, local img = 'bg') e contas (+ - * / %) valem nos argumentos.
 */

#include "logic/stage/StageLuaParse.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAGE_TAG_MAX    64
#define STAGE_MAX_VARS   48
#define STAGE_MAX_BLOCKS 64

typedef struct StageTagMap {
    char tag[STAGE_TAG_MAX];
    int index;
} StageTagMap;

typedef enum StageLuaCall {
    LUA_CALL_NONE = 0,
    LUA_CALL_MAKE_SPRITE,
    LUA_CALL_MAKE_ANIMATED_SPRITE,
    LUA_CALL_MAKE_GRAPHIC,
    LUA_CALL_LOAD_GRAPHIC,
    LUA_CALL_SET_SCROLL,
    LUA_CALL_SCALE_OBJECT,
    LUA_CALL_SCALE_LUA,
    LUA_CALL_GRAPHIC_SIZE,
    LUA_CALL_UPDATE_HITBOX,
    LUA_CALL_SCREEN_CENTER,
    LUA_CALL_ADD_SPRITE,
    LUA_CALL_REMOVE_SPRITE,
    LUA_CALL_SET_OBJECT_ORDER,
    LUA_CALL_ADD_ANIM_PREFIX,
    LUA_CALL_ADD_ANIM_INDICES,
    LUA_CALL_ADD_ANIM_INDICES_LOOP,
    LUA_CALL_ADD_OFFSET,
    LUA_CALL_PLAY_ANIM,
    LUA_CALL_SET_PROPERTY,
    LUA_CALL_GLITCH,
    LUA_CALL_IGNORED        /* conhecidas, sem efeito aqui (camera, blend, shader) */
} StageLuaCall;

typedef enum { CTX_SETUP, CTX_RUNTIME, CTX_COUNTDOWN, CTX_BEAT, CTX_STEP, CTX_SECTION } StageCtx;
enum { BLOCK_FUNC, BLOCK_IF, BLOCK_OTHER };

typedef struct {
    char name[32];
    uint8_t isText;
    double number;
    char text[128];
} StageLuaVar;

/* if/elseif/else: own = condicao do ramo atual, neg = negacao dos ramos anteriores. */
typedef struct {
    uint8_t type;
    uint8_t ownCount, negCount;
    bool ownNegatable;
    StageCond own[STAGE_COND_MAX];
    StageCond neg[STAGE_COND_MAX];
} StageBlock;

typedef struct {
    StageSprite *out;
    int capacity, objectCount, mapCount;
    StageTagMap maps[STAGE_MAX_SPRITES];
    int addSeq[STAGE_MAX_SPRITES];   /* ordem do addLuaSprite (0 = nao adicionado) */
    int addCounter;

    StageScript *script;             /* NULL = so sprites (editor) */
    StageLuaVar vars[STAGE_MAX_VARS];
    int varCount;

    StageBlock blocks[STAGE_MAX_BLOCKS];
    int depth, funcDepth;
    StageCtx ctx;
    char counterName[32];            /* parametro do onCountdownTick */

    int glitch, ignored;
} StageLuaState;

typedef struct {
    const char *p, *end;
} StageArgs;

static inline bool Stage_TokenEquals(const char *token, size_t tokenLength, const char *literal)
{
    const size_t literalLength = strlen(literal);
    return tokenLength == literalLength && memcmp(token, literal, literalLength) == 0;
}

static inline bool Stage_IsIdent(char c) { return isalnum((unsigned char)c) || c == '_'; }

/* Tabela por nome. Roda so no load do stage. */
static StageLuaCall Stage_GetCall(const char *token, size_t length)
{
    static const struct { const char *name; StageLuaCall call; } calls[] = {
        { "makeLuaSprite",                  LUA_CALL_MAKE_SPRITE },
        { "makeAnimatedLuaSprite",          LUA_CALL_MAKE_ANIMATED_SPRITE },
        { "makeGraphic",                    LUA_CALL_MAKE_GRAPHIC },
        { "loadGraphic",                    LUA_CALL_LOAD_GRAPHIC },
        { "setLuaSpriteScrollFactor",       LUA_CALL_SET_SCROLL },
        { "setScrollFactor",                LUA_CALL_SET_SCROLL },
        { "scaleObject",                    LUA_CALL_SCALE_OBJECT },
        { "scaleLuaSprite",                 LUA_CALL_SCALE_LUA },
        { "setGraphicSize",                 LUA_CALL_GRAPHIC_SIZE },
        { "updateHitbox",                   LUA_CALL_UPDATE_HITBOX },
        { "screenCenter",                   LUA_CALL_SCREEN_CENTER },
        { "addLuaSprite",                   LUA_CALL_ADD_SPRITE },
        { "removeLuaSprite",                LUA_CALL_REMOVE_SPRITE },
        { "setObjectOrder",                 LUA_CALL_SET_OBJECT_ORDER },
        { "addAnimationByPrefix",           LUA_CALL_ADD_ANIM_PREFIX },
        { "luaSpriteAddAnimationByPrefix",  LUA_CALL_ADD_ANIM_PREFIX },
        { "addAnimationByIndices",          LUA_CALL_ADD_ANIM_INDICES },
        { "luaSpriteAddAnimationByIndices", LUA_CALL_ADD_ANIM_INDICES },
        { "addAnimationByIndicesLoop",      LUA_CALL_ADD_ANIM_INDICES_LOOP },
        { "addOffset",                      LUA_CALL_ADD_OFFSET },
        { "playAnim",                       LUA_CALL_PLAY_ANIM },
        { "objectPlayAnimation",            LUA_CALL_PLAY_ANIM },
        { "luaSpritePlayAnimation",         LUA_CALL_PLAY_ANIM },
        { "setProperty",                    LUA_CALL_SET_PROPERTY },
        { "addGlitchEffect",                LUA_CALL_GLITCH },
        { "setObjectCamera",                LUA_CALL_IGNORED },
        { "setBlendMode",                   LUA_CALL_IGNORED },
        { "setSpriteShader",                LUA_CALL_IGNORED },
    };

    for (size_t i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
        if (Stage_TokenEquals(token, length, calls[i].name))
            return calls[i].call;
    }
    return LUA_CALL_NONE;
}

/* Callback da Psych pelo nome da funcao (so funcoes de topo). */
static StageCtx Stage_ClassifyFunction(const char *name, size_t length)
{
    static const struct { const char *name; StageCtx ctx; } table[] = {
        { "onCreate",           CTX_SETUP },
        { "onCreatePost",       CTX_SETUP },
        { "onSongStart",        CTX_SETUP },
        { "onCountdownStarted", CTX_SETUP },
        { "onStartCountdown",   CTX_SETUP },
        { "onCountdownTick",    CTX_COUNTDOWN },
        { "onBeatHit",          CTX_BEAT },
        { "onStepHit",          CTX_STEP },
        { "onSectionHit",       CTX_SECTION },
        { "goodNoteHit",        CTX_RUNTIME },
        { "opponentNoteHit",    CTX_RUNTIME },
        { "noteMiss",           CTX_RUNTIME },
        { "noteMissPress",      CTX_RUNTIME },
        { "eventEarlyTrigger",  CTX_RUNTIME },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (Stage_TokenEquals(name, length, table[i].name))
            return table[i].ctx;
    }
    /* Outras callbacks (onUpdate, onEvent, onTimerCompleted...) rodam durante o jogo. */
    if (length > 2 && name[0] == 'o' && name[1] == 'n' && isupper((unsigned char)name[2]))
        return CTX_RUNTIME;
    return CTX_SETUP; /* funcao auxiliar: tratada como parte do create */
}

/* ------------------------------------------------------------------------- */
/* Texto                                                                     */
/* ------------------------------------------------------------------------- */

static const char *Stage_SkipQuotedString(const char *p)
{
    const char quote = *p++;
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p++ == quote)
            break;
    }
    return p;
}

static const char *Stage_SkipLongBracket(const char *p)
{
    if (*p != '[')
        return p;

    const char *q = p + 1;
    while (*q == '=')
        ++q;

    if (*q != '[')
        return p;

    const size_t equalsCount = (size_t)(q - (p + 1));
    p = q + 1;

    while (*p != '\0') {
        if (*p == ']') {
            const char *close = p + 1;
            size_t i = 0;
            while (i < equalsCount && close[i] == '=')
                ++i;
            if (i == equalsCount && close[i] == ']')
                return close + i + 1;
        }
        ++p;
    }
    return p;
}

static inline const char *Stage_SkipComment(const char *p)
{
    const char *longText = Stage_SkipLongBracket(p + 2);
    if (longText != p + 2)
        return longText;

    while (*p != '\0' && *p != '\n')
        ++p;
    return p;
}

/* Pula string/comentario/colchete longo em p. Devolve p se nao havia nada para pular. */
static const char *Stage_SkipNonCode(const char *p)
{
    if (*p == '\'' || *p == '"')
        return Stage_SkipQuotedString(p);
    if (p[0] == '-' && p[1] == '-')
        return Stage_SkipComment(p);
    if (*p == '[')
        return Stage_SkipLongBracket(p);
    return p;
}

static const char *Stage_FindCallEnd(const char *openParen)
{
    unsigned depth = 1;
    const char *p = openParen + 1;

    while (*p != '\0') {
        const char *skipped = Stage_SkipNonCode(p);
        if (skipped != p) {
            p = skipped;
            continue;
        }
        if (*p == '(') {
            ++depth;
        } else if (*p == ')') {
            if (--depth == 0)
                return p + 1;
        }
        ++p;
    }
    return NULL;
}

static inline void Stage_Trim(const char **s, const char **e)
{
    while (*s < *e && isspace((unsigned char)**s))
        ++*s;
    while (*e > *s && isspace((unsigned char)(*e)[-1]))
        --*e;
}

/* Proximo argumento da chamada em [*s, *e), sem espacos. false = acabaram. */
static bool Stage_NextArg(StageArgs *a, const char **s, const char **e)
{
    const char *p = a->p;
    while (p < a->end && isspace((unsigned char)*p))
        ++p;
    if (p >= a->end)
        return false;

    const char *start = p;
    int depth = 0;
    while (p < a->end) {
        if (*p == '\'' || *p == '"') {
            p = Stage_SkipQuotedString(p);
            continue;
        }
        if (*p == '(' || *p == '{' || *p == '[')
            ++depth;
        else if (*p == ')' || *p == '}' || *p == ']')
            --depth;
        else if (*p == ',' && depth <= 0)
            break;
        ++p;
    }
    if (p > a->end)
        p = a->end;

    *s = start;
    *e = p;
    Stage_Trim(s, e);
    a->p = p < a->end ? p + 1 : p;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Valores: variaveis, numeros, textos, booleanos, cores                     */
/* ------------------------------------------------------------------------- */

static const StageLuaVar *Stage_FindVar(const StageLuaState *st, const char *name, size_t length)
{
    for (int i = st->varCount - 1; i >= 0; --i) {
        if (Stage_TokenEquals(name, length, st->vars[i].name))
            return &st->vars[i];
    }
    return NULL;
}

typedef struct {
    const char *p, *end;
    const StageLuaState *st;
    bool ok;
} StageExpr;

static void Expr_Space(StageExpr *x)
{
    while (x->p < x->end && isspace((unsigned char)*x->p))
        ++x->p;
}

static double Expr_Sum(StageExpr *x);

static double Expr_Primary(StageExpr *x)
{
    Expr_Space(x);
    if (x->p >= x->end) {
        x->ok = false;
        return 0.0;
    }

    const char c = *x->p;
    if (c == '(') {
        ++x->p;
        const double value = Expr_Sum(x);
        Expr_Space(x);
        if (x->p < x->end && *x->p == ')')
            ++x->p;
        else
            x->ok = false;
        return value;
    }

    if (isdigit((unsigned char)c) || (c == '.' && x->p + 1 < x->end && isdigit((unsigned char)x->p[1]))) {
        char *numberEnd = NULL;
        const double value = strtod(x->p, &numberEnd); /* aceita 0xFF como o Lua */
        if (numberEnd == x->p || numberEnd > x->end) {
            x->ok = false;
            return 0.0;
        }
        x->p = numberEnd;
        return value;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        const char *name = x->p;
        while (x->p < x->end && Stage_IsIdent(*x->p))
            ++x->p;
        const size_t length = (size_t)(x->p - name);

        const StageLuaVar *var = Stage_FindVar(x->st, name, length);
        if (var != NULL && !var->isText)
            return var->number;
        if (Stage_TokenEquals(name, length, "screenWidth"))
            return 1280.0;
        if (Stage_TokenEquals(name, length, "screenHeight"))
            return 720.0;
    }

    x->ok = false;
    return 0.0;
}

static double Expr_Unary(StageExpr *x)
{
    Expr_Space(x);
    if (x->p < x->end && *x->p == '-') {
        ++x->p;
        return -Expr_Unary(x);
    }
    return Expr_Primary(x);
}

static double Expr_Product(StageExpr *x)
{
    double value = Expr_Unary(x);
    for (;;) {
        Expr_Space(x);
        if (!x->ok || x->p >= x->end)
            return value;

        const char op = *x->p;
        if (op != '*' && op != '/' && op != '%')
            return value;
        ++x->p;

        const double rhs = Expr_Unary(x);
        if (op == '*')
            value *= rhs;
        else if (rhs == 0.0)
            x->ok = false;
        else if (op == '/')
            value /= rhs;
        else
            value -= floor(value / rhs) * rhs; /* % do Lua (arredonda para baixo) */
    }
}

static double Expr_Sum(StageExpr *x)
{
    double value = Expr_Product(x);
    for (;;) {
        Expr_Space(x);
        if (!x->ok || x->p >= x->end)
            return value;

        const char op = *x->p;
        if (op != '+' && op != '-')
            return value;
        ++x->p;

        const double rhs = Expr_Product(x);
        value = op == '+' ? value + rhs : value - rhs;
    }
}

static bool Stage_EvalNumber(const StageLuaState *st, const char *s, const char *e, double *out)
{
    StageExpr x = { s, e, st, true };
    const double value = Expr_Sum(&x);
    Expr_Space(&x);
    if (!x.ok || x.p != e || !isfinite(value))
        return false;
    *out = value;
    return true;
}

static bool Stage_EvalFloat(const StageLuaState *st, const char *s, const char *e, float *out)
{
    double value = 0.0;
    if (!Stage_EvalNumber(st, s, e, &value))
        return false;
    *out = (float)value;
    return true;
}

/* 'texto', "texto" ou variavel, juntos com "..". Texto vazio e valido. */
static bool Stage_EvalString(const StageLuaState *st, const char *s, const char *e, char *out, size_t capacity)
{
    size_t written = 0;
    const char *p = s;

    if (capacity == 0)
        return false;

    for (;;) {
        while (p < e && isspace((unsigned char)*p))
            ++p;
        if (p >= e)
            return false;

        if (*p == '\'' || *p == '"') {
            const char quote = *p++;
            bool closed = false;
            while (p < e) {
                char c = *p++;
                if (c == quote) {
                    closed = true;
                    break;
                }
                if (c == '\\' && p < e) {
                    c = *p++;
                    switch (c) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        default: break;
                    }
                }
                if (written + 1 >= capacity)
                    return false;
                out[written++] = c;
            }
            if (!closed)
                return false;
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            const char *name = p;
            while (p < e && Stage_IsIdent(*p))
                ++p;
            const StageLuaVar *var = Stage_FindVar(st, name, (size_t)(p - name));
            if (var == NULL)
                return false;

            char number[32];
            const char *text = var->text;
            if (!var->isText) {
                snprintf(number, sizeof(number), "%.14g", var->number);
                text = number;
            }
            const size_t length = strlen(text);
            if (written + length >= capacity)
                return false;
            memcpy(out + written, text, length);
            written += length;
        } else {
            return false;
        }

        while (p < e && isspace((unsigned char)*p))
            ++p;
        if (p >= e)
            break;
        if (p + 1 < e && p[0] == '.' && p[1] == '.') {
            p += 2;
            continue;
        }
        return false;
    }

    out[written] = '\0';
    return true;
}

static bool Stage_EvalBool(const char *s, const char *e, bool *out)
{
    if (Stage_TokenEquals(s, (size_t)(e - s), "true")) {
        *out = true;
        return true;
    }
    if (Stage_TokenEquals(s, (size_t)(e - s), "false")) {
        *out = false;
        return true;
    }
    return false;
}

static inline int Stage_HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "RRGGBB" / "RRGGBBAA" com # ou 0x opcional. */
static bool Stage_ParseColor(const char *text, uint8_t rgba[4])
{
    if (text[0] == '#')
        ++text;
    else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        text += 2;

    const size_t length = strlen(text);
    if (length != 6 && length != 8)
        return false;

    uint8_t channels[4] = {0, 0, 0, 255};
    for (size_t i = 0; i < length / 2; ++i) {
        const int hi = Stage_HexDigit(text[i * 2]);
        const int lo = Stage_HexDigit(text[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        channels[i] = (uint8_t)((hi << 4) | lo);
    }
    memcpy(rgba, channels, 4);
    return true;
}

/* Texto hex, getColorFromHex('FF0000') ou numero 0xRRGGBB / 0xAARRGGBB (FlxColor). */
static bool Stage_EvalColor(const StageLuaState *st, const char *s, const char *e, uint8_t rgba[4])
{
    char text[32];
    if (Stage_EvalString(st, s, e, text, sizeof(text)))
        return Stage_ParseColor(text, rgba);

    static const char fromHex[] = "getColorFromHex";
    const size_t fromHexLength = sizeof(fromHex) - 1;
    if ((size_t)(e - s) > fromHexLength + 2 && memcmp(s, fromHex, fromHexLength) == 0) {
        const char *open = s + fromHexLength;
        while (open < e && isspace((unsigned char)*open))
            ++open;
        if (open < e && *open == '(' && e[-1] == ')' &&
            Stage_EvalString(st, open + 1, e - 1, text, sizeof(text)))
            return Stage_ParseColor(text, rgba);
        return false;
    }

    double number = 0.0;
    if (!Stage_EvalNumber(st, s, e, &number) || number < 0.0 || number > 4294967295.0)
        return false;
    const uint32_t value = (uint32_t)number;
    rgba[0] = (uint8_t)(value >> 16);
    rgba[1] = (uint8_t)(value >> 8);
    rgba[2] = (uint8_t)value;
    rgba[3] = value > 0xFFFFFFu ? (uint8_t)(value >> 24) : 255;
    return true;
}

/* Proximo argumento como texto/numero/booleano. false = sem argumento ou valor desconhecido. */
static bool Stage_ArgString(const StageLuaState *st, StageArgs *a, char *out, size_t capacity)
{
    const char *s, *e;
    return Stage_NextArg(a, &s, &e) && Stage_EvalString(st, s, e, out, capacity);
}

static bool Stage_ArgFloat(const StageLuaState *st, StageArgs *a, float *out)
{
    const char *s, *e;
    return Stage_NextArg(a, &s, &e) && Stage_EvalFloat(st, s, e, out);
}

static bool Stage_ArgBool(StageArgs *a, bool *out)
{
    const char *s, *e;
    return Stage_NextArg(a, &s, &e) && Stage_EvalBool(s, e, out);
}

/* ------------------------------------------------------------------------- */
/* Variaveis                                                                 */
/* ------------------------------------------------------------------------- */

/* Fim do valor de "x = valor": fim da linha, ';' ou comentario fora de parenteses. */
static const char *Stage_StatementEnd(const char *p)
{
    int depth = 0;
    while (*p != '\0') {
        if (*p == '\'' || *p == '"') {
            p = Stage_SkipQuotedString(p);
            continue;
        }
        if (depth <= 0 && (*p == '\n' || *p == ';' || (p[0] == '-' && p[1] == '-')))
            break;
        if (*p == '(' || *p == '{')
            ++depth;
        else if (*p == ')' || *p == '}')
            --depth;
        ++p;
    }
    return p;
}

/* "nome = valor": guarda numero ou texto; valor desconhecido apaga a variavel. */
static void Stage_Assign(StageLuaState *st, const char *name, size_t length, const char *s, const char *e)
{
    if (length == 0 || length >= sizeof(st->vars[0].name))
        return;
    Stage_Trim(&s, &e);

    StageLuaVar var;
    memset(&var, 0, sizeof(var));
    memcpy(var.name, name, length);

    bool known = Stage_EvalNumber(st, s, e, &var.number);
    if (!known) {
        known = Stage_EvalString(st, s, e, var.text, sizeof(var.text));
        var.isText = 1;
    }

    int slot = -1;
    for (int i = 0; i < st->varCount; ++i) {
        if (strcmp(st->vars[i].name, var.name) == 0) {
            slot = i;
            break;
        }
    }

    if (!known) {
        if (slot >= 0)
            st->vars[slot] = st->vars[--st->varCount];
        return;
    }
    if (slot < 0) {
        if (st->varCount >= STAGE_MAX_VARS)
            return;
        slot = st->varCount++;
    }
    st->vars[slot] = var;
}

/* ------------------------------------------------------------------------- */
/* Condicoes (if curBeat % 2 == 0 then)                                      */
/* ------------------------------------------------------------------------- */

static bool Stage_CondVar(const StageLuaState *st, const char *name, size_t length, uint8_t *var)
{
    if (Stage_TokenEquals(name, length, "curBeat")) { *var = STAGE_VAR_BEAT; return true; }
    if (Stage_TokenEquals(name, length, "curStep")) { *var = STAGE_VAR_STEP; return true; }
    if (Stage_TokenEquals(name, length, "curSection")) { *var = STAGE_VAR_SECTION; return true; }
    if (st->counterName[0] != '\0' && Stage_TokenEquals(name, length, st->counterName)) {
        *var = STAGE_VAR_COUNTER;
        return true;
    }
    return false;
}

/* Tira parenteses que envolvem tudo: "((a))" -> "a". */
static void Stage_TrimParens(const char **s, const char **e)
{
    Stage_Trim(s, e);
    while (*e - *s >= 2 && **s == '(' && (*e)[-1] == ')') {
        int depth = 0;
        const char *p = *s;
        for (; p < *e; ++p) {
            if (*p == '(') ++depth;
            else if (*p == ')' && --depth == 0) break;
        }
        if (p != *e - 1)
            return;
        ++*s;
        --*e;
        Stage_Trim(s, e);
    }
}

/* "curBeat % 2 == 0", "(curStep % 4) ~= 1", "curBeat >= 16". */
static bool Stage_ParseCompare(const StageLuaState *st, const char *s, const char *e, StageCond *out)
{
    Stage_TrimParens(&s, &e);
    const char *p = s;
    bool paren = false;

    if (p < e && *p == '(') {
        paren = true;
        ++p;
        while (p < e && isspace((unsigned char)*p)) ++p;
    }

    const char *name = p;
    while (p < e && Stage_IsIdent(*p))
        ++p;
    StageCond cond = {0};
    if (!Stage_CondVar(st, name, (size_t)(p - name), &cond.var))
        return false;

    while (p < e && isspace((unsigned char)*p)) ++p;
    if (p < e && *p == '%') {
        ++p;
        char *numberEnd = NULL;
        const long mod = strtol(p, &numberEnd, 10);
        if (numberEnd == p || numberEnd > e || mod <= 0 || mod > INT16_MAX)
            return false;
        cond.mod = (int16_t)mod;
        p = numberEnd;
        while (p < e && isspace((unsigned char)*p)) ++p;
    }
    if (paren) {
        if (p >= e || *p != ')')
            return false;
        ++p;
        while (p < e && isspace((unsigned char)*p)) ++p;
    }

    if (e - p < 2)
        return false;
    if (p[0] == '=' && p[1] == '=')      { cond.op = STAGE_OP_EQ; p += 2; }
    else if (p[0] == '~' && p[1] == '=') { cond.op = STAGE_OP_NE; p += 2; }
    else if (p[0] == '<' && p[1] == '=') { cond.op = STAGE_OP_LE; p += 2; }
    else if (p[0] == '>' && p[1] == '=') { cond.op = STAGE_OP_GE; p += 2; }
    else if (p[0] == '<')                { cond.op = STAGE_OP_LT; p += 1; }
    else if (p[0] == '>')                { cond.op = STAGE_OP_GT; p += 1; }
    else return false;

    double value = 0.0;
    if (!Stage_EvalNumber(st, p, e, &value) || value < INT32_MIN || value > INT32_MAX)
        return false;
    cond.value = (int32_t)value;
    *out = cond;
    return true;
}

/* Comparacoes unidas por "and". *exact = a condicao inteira foi entendida. */
static int Stage_ParseCondition(const StageLuaState *st, const char *s, const char *e,
                                StageCond out[STAGE_COND_MAX], bool *exact)
{
    int count = 0;
    const char *part = s;
    *exact = true;

    while (part < e) {
        const char *partEnd = e, *next = e;
        bool partKnown = true;
        int depth = 0;

        for (const char *q = part; q < e;) {
            if (*q == '\'' || *q == '"') {
                q = Stage_SkipQuotedString(q);
                continue;
            }
            if (*q == '(') ++depth;
            else if (*q == ')') --depth;

            if ((isalpha((unsigned char)*q) || *q == '_') && (q == s || !Stage_IsIdent(q[-1]))) {
                const char *word = q;
                while (q < e && Stage_IsIdent(*q))
                    ++q;
                const size_t length = (size_t)(q - word);
                if (Stage_TokenEquals(word, length, "or")) {
                    *exact = false;
                    return 0;
                }
                if (Stage_TokenEquals(word, length, "not"))
                    partKnown = false;
                if (depth == 0 && Stage_TokenEquals(word, length, "and")) {
                    partEnd = word;
                    next = q;
                    break;
                }
                continue;
            }
            ++q;
        }

        StageCond cond;
        if (partKnown && Stage_ParseCompare(st, part, partEnd, &cond) && count < STAGE_COND_MAX)
            out[count++] = cond;
        else
            *exact = false;
        part = next;
    }
    return count;
}

static StageOp Stage_NegateOp(uint8_t op)
{
    switch (op) {
        case STAGE_OP_EQ: return STAGE_OP_NE;
        case STAGE_OP_NE: return STAGE_OP_EQ;
        case STAGE_OP_LT: return STAGE_OP_GE;
        case STAGE_OP_LE: return STAGE_OP_GT;
        case STAGE_OP_GT: return STAGE_OP_LE;
        default:          return STAGE_OP_LT;
    }
}

int Stage_CondsMatch(const StageCond *cond, int count, int beat, int step, int section, int counter)
{
    for (int i = 0; i < count; ++i) {
        const StageCond *c = &cond[i];
        long long value = c->var == STAGE_VAR_BEAT ? beat
                        : c->var == STAGE_VAR_STEP ? step
                        : c->var == STAGE_VAR_SECTION ? section : counter;
        if (c->mod > 0) {
            value %= c->mod;
            if (value < 0)
                value += c->mod; /* % do Lua nunca e negativo */
        }

        bool ok;
        switch (c->op) {
            case STAGE_OP_EQ: ok = value == c->value; break;
            case STAGE_OP_NE: ok = value != c->value; break;
            case STAGE_OP_LT: ok = value < c->value; break;
            case STAGE_OP_LE: ok = value <= c->value; break;
            case STAGE_OP_GT: ok = value > c->value; break;
            default:          ok = value >= c->value; break;
        }
        if (!ok)
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Blocos                                                                    */
/* ------------------------------------------------------------------------- */

static StageBlock *Stage_TopBlock(StageLuaState *st)
{
    return st->depth > 0 && st->depth <= STAGE_MAX_BLOCKS ? &st->blocks[st->depth - 1] : NULL;
}

static StageBlock *Stage_PushBlock(StageLuaState *st, uint8_t type)
{
    if (st->depth++ >= STAGE_MAX_BLOCKS)
        return NULL; /* fundo demais: so conta para casar o "end" */
    StageBlock *block = &st->blocks[st->depth - 1];
    memset(block, 0, sizeof(*block));
    block->type = type;
    return block;
}

static void Stage_PopBlock(StageLuaState *st)
{
    if (st->depth <= 0)
        return;
    const StageBlock *block = Stage_TopBlock(st);
    --st->depth;
    if (block != NULL && block->type == BLOCK_FUNC && --st->funcDepth <= 0) {
        st->funcDepth = 0;
        st->ctx = CTX_SETUP;
        st->counterName[0] = '\0';
    }
}

/* Depois de "function": nome opcional (a.b:c) e parametros. Devolve o ponteiro depois do ')'. */
static const char *Stage_EnterFunction(StageLuaState *st, const char *p)
{
    while (isspace((unsigned char)*p))
        ++p;
    const char *name = p;
    while (Stage_IsIdent(*p) || *p == '.' || *p == ':')
        ++p;
    const size_t nameLength = (size_t)(p - name);

    char param[sizeof(st->counterName)] = {0};
    while (isspace((unsigned char)*p))
        ++p;
    if (*p == '(') {
        ++p;
        while (isspace((unsigned char)*p))
            ++p;
        const char *paramStart = p;
        while (Stage_IsIdent(*p))
            ++p;
        size_t paramLength = (size_t)(p - paramStart);
        if (paramLength >= sizeof(param))
            paramLength = sizeof(param) - 1;
        memcpy(param, paramStart, paramLength);
        while (*p != '\0' && *p != ')')
            ++p;
        if (*p == ')')
            ++p;
    }

    if (Stage_PushBlock(st, BLOCK_FUNC) == NULL)
        return p;
    if (st->funcDepth++ == 0) {
        st->ctx = nameLength > 0 ? Stage_ClassifyFunction(name, nameLength) : CTX_SETUP;
        if (st->ctx == CTX_RUNTIME)
            ++st->ignored;
        snprintf(st->counterName, sizeof(st->counterName), "%s", st->ctx == CTX_COUNTDOWN ? param : "");
    }
    return p;
}

/* Procura o "then" do if. NULL se nao achou. */
static const char *Stage_FindThen(const char *p, const char **afterThen)
{
    while (*p != '\0') {
        const char *skipped = Stage_SkipNonCode(p);
        if (skipped != p) {
            p = skipped;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *word = p;
            while (Stage_IsIdent(*p))
                ++p;
            if (Stage_TokenEquals(word, (size_t)(p - word), "then")) {
                *afterThen = p;
                return word;
            }
            continue;
        }
        ++p;
    }
    return NULL;
}

static const char *Stage_EnterIf(StageLuaState *st, const char *p, bool isElseif)
{
    const char *afterThen = NULL;
    const char *thenWord = Stage_FindThen(p, &afterThen);
    if (thenWord == NULL)
        return p;

    StageBlock *block;
    if (isElseif) {
        block = Stage_TopBlock(st);
        if (block == NULL || block->type != BLOCK_IF)
            return afterThen;
        if (block->ownNegatable && block->negCount < STAGE_COND_MAX) {
            StageCond cond = block->own[0];
            cond.op = (uint8_t)Stage_NegateOp(cond.op);
            block->neg[block->negCount++] = cond;
        }
    } else {
        block = Stage_PushBlock(st, BLOCK_IF);
        if (block == NULL)
            return afterThen;
    }

    bool exact = false;
    block->ownCount = (uint8_t)Stage_ParseCondition(st, p, thenWord, block->own, &exact);
    block->ownNegatable = exact && block->ownCount == 1;
    return afterThen;
}

static void Stage_EnterElse(StageLuaState *st)
{
    StageBlock *block = Stage_TopBlock(st);
    if (block == NULL || block->type != BLOCK_IF)
        return;
    if (block->ownNegatable && block->negCount < STAGE_COND_MAX) {
        StageCond cond = block->own[0];
        cond.op = (uint8_t)Stage_NegateOp(cond.op);
        block->neg[block->negCount++] = cond;
    }
    block->ownCount = 0;
    block->ownNegatable = false;
}

/* Condicoes de todos os if abertos (condicao desconhecida = sempre verdadeira). */
static int Stage_CollectConds(const StageLuaState *st, StageCond out[STAGE_COND_MAX])
{
    int count = 0;
    const int depth = st->depth < STAGE_MAX_BLOCKS ? st->depth : STAGE_MAX_BLOCKS;
    for (int i = 0; i < depth; ++i) {
        const StageBlock *block = &st->blocks[i];
        if (block->type != BLOCK_IF)
            continue;
        for (int n = 0; n < block->negCount && count < STAGE_COND_MAX; ++n)
            out[count++] = block->neg[n];
        for (int n = 0; n < block->ownCount && count < STAGE_COND_MAX; ++n)
            out[count++] = block->own[n];
    }
    return count;
}

/* ------------------------------------------------------------------------- */
/* Sprites                                                                   */
/* ------------------------------------------------------------------------- */

static int Stage_FindSprite(const StageLuaState *st, const char *tag)
{
    for (int i = 0; i < st->mapCount; ++i) {
        if (strcmp(st->maps[i].tag, tag) == 0)
            return st->maps[i].index;
    }
    return -1;
}

static void Stage_ResetSprite(StageLuaState *st, int index)
{
    StageSprite *sprite = &st->out[index];
    memset(sprite, 0, sizeof(*sprite));
    sprite->scrollX = sprite->scrollY = sprite->scaleX = sprite->scaleY = 1.0f;
    sprite->colorR = sprite->colorG = sprite->colorB = sprite->colorA = 255;
    st->addSeq[index] = 0;
}

/* *created = 1 se o sprite e novo. -1 sem espaco. */
static int Stage_GetOrCreateSprite(StageLuaState *st, const char *tag, bool *created)
{
    *created = false;
    const int found = Stage_FindSprite(st, tag);
    if (found >= 0)
        return found;

    if (st->objectCount >= st->capacity || st->mapCount >= STAGE_MAX_SPRITES)
        return -1;

    const int index = st->objectCount++;
    Stage_ResetSprite(st, index);
    snprintf(st->maps[st->mapCount].tag, sizeof(st->maps[0].tag), "%s", tag);
    st->maps[st->mapCount++].index = index;
    *created = true;
    return index;
}

static StageAnimDef *Stage_FindAnimDef(StageLuaState *st, int sprite, const char *name)
{
    if (st->script == NULL)
        return NULL;
    for (int i = 0; i < st->script->animCount; ++i) {
        StageAnimDef *def = &st->script->anims[i];
        if (def->sprite == sprite && strcmp(def->name, name) == 0)
            return def;
    }
    return NULL;
}

static void Stage_AddAnim(StageLuaState *st, int index, const char *name, const char *prefix,
                          const char *indices, float fps, bool loop)
{
    StageSprite *sprite = &st->out[index];
    if (fps <= 0.0f)
        fps = 24.0f;

    /* Campos do editor: so a primeira animacao. */
    if (sprite->animPrefix[0] == '\0') {
        snprintf(sprite->animPrefix, sizeof(sprite->animPrefix), "%s", prefix);
        sprite->animFps = fps;
        sprite->animLoop = loop ? 1 : 0;
    }

    if (st->script == NULL)
        return;

    StageAnimDef *def = Stage_FindAnimDef(st, index, name); /* mesmo nome substitui, como na Psych */
    if (def == NULL) {
        if (st->script->animCount >= STAGE_MAX_ANIM_DEFS) {
            ++st->ignored;
            return;
        }
        def = &st->script->anims[st->script->animCount++];
    }
    memset(def, 0, sizeof(*def));
    def->sprite = (uint8_t)index;
    def->loop = loop ? 1 : 0;
    def->fps = fps;
    snprintf(def->name, sizeof(def->name), "%s", name);
    snprintf(def->prefix, sizeof(def->prefix), "%s", prefix);
    snprintf(def->indices, sizeof(def->indices), "%s", indices);
}

static void Stage_PlayAnim(StageLuaState *st, int index, StageArgs *a)
{
    static const uint8_t triggers[] = {
        [CTX_SETUP] = STAGE_TRIGGER_CREATE, [CTX_RUNTIME] = STAGE_TRIGGER_CREATE,
        [CTX_COUNTDOWN] = STAGE_TRIGGER_COUNTDOWN, [CTX_BEAT] = STAGE_TRIGGER_BEAT,
        [CTX_STEP] = STAGE_TRIGGER_STEP, [CTX_SECTION] = STAGE_TRIGGER_SECTION,
    };

    char anim[32];
    bool forced = false;
    if (st->script == NULL)
        return;
    if (index < 0 || !Stage_ArgString(st, a, anim, sizeof(anim)) || st->script->actionCount >= STAGE_MAX_ACTIONS) {
        ++st->ignored;
        return;
    }
    (void)Stage_ArgBool(a, &forced);

    StageAnimAction *action = &st->script->actions[st->script->actionCount++];
    memset(action, 0, sizeof(*action));
    action->sprite = (uint8_t)index;
    action->trigger = triggers[st->ctx];
    action->forced = forced ? 1 : 0;
    if (st->ctx != CTX_SETUP)
        action->condCount = (uint8_t)Stage_CollectConds(st, action->cond);
    snprintf(action->anim, sizeof(action->anim), "%s", anim);
}

/* setProperty('tag.campo', valor) e alguns campos do PlayState. */
static void Stage_SetProperty(StageLuaState *st, StageArgs *a)
{
    char prop[STAGE_TAG_MAX + 32];
    const char *s, *e;
    if (!Stage_ArgString(st, a, prop, sizeof(prop)) || !Stage_NextArg(a, &s, &e)) {
        ++st->ignored;
        return;
    }

    float number = 0.0f;
    bool flag = false;
    const bool isNumber = Stage_EvalFloat(st, s, e, &number);
    const bool isBool = Stage_EvalBool(s, e, &flag);

    char *dot = strchr(prop, '.');
    if (dot == NULL) {
        if (strcmp(prop, "defaultCamZoom") == 0 && isNumber && number > 0.0f && st->script != NULL)
            st->script->camZoom = number;
        else
            ++st->ignored;
        return;
    }
    *dot = '\0';
    const char *field = dot + 1;

    if (strcmp(prop, "gf") == 0 && strcmp(field, "visible") == 0 && isBool) {
        if (!flag && st->script != NULL)
            st->script->hideGirlfriend = 1;
        return;
    }

    const int index = Stage_FindSprite(st, prop);
    if (index < 0) {
        ++st->ignored;
        return;
    }
    StageSprite *sprite = &st->out[index];

    if (isNumber && strcmp(field, "x") == 0) {
        sprite->x = number;
        sprite->center &= (uint8_t)~1u;
    } else if (isNumber && strcmp(field, "y") == 0) {
        sprite->y = number;
        sprite->center &= (uint8_t)~2u;
    } else if (isNumber && (strcmp(field, "scale.x") == 0 || strcmp(field, "scale.y") == 0)) {
        /* Sem updateHitbox a Psych cresce do centro. */
        if (field[6] == 'x') sprite->scaleX = number;
        else sprite->scaleY = number;
        sprite->scaleCenter = 1;
        sprite->sizeW = sprite->sizeH = 0.0f;
    } else if (isNumber && strcmp(field, "scrollFactor.x") == 0) {
        sprite->scrollX = number;
    } else if (isNumber && strcmp(field, "scrollFactor.y") == 0) {
        sprite->scrollY = number;
    } else if (isNumber && strcmp(field, "alpha") == 0) {
        const float alpha = number < 0.0f ? 0.0f : number > 1.0f ? 1.0f : number;
        sprite->colorA = (uint8_t)lroundf(alpha * 255.0f);
    } else if (isBool && strcmp(field, "visible") == 0) {
        sprite->hidden = flag ? 0 : 1;
    } else if (isBool && strcmp(field, "flipX") == 0) {
        sprite->flipX = flag ? 1 : 0;
    } else if (strcmp(field, "color") == 0) {
        uint8_t rgba[4];
        if (Stage_EvalColor(st, s, e, rgba)) {
            sprite->colorR = rgba[0];
            sprite->colorG = rgba[1];
            sprite->colorB = rgba[2];
        } else {
            ++st->ignored;
        }
    } else if (strcmp(field, "antialiasing") != 0) {
        ++st->ignored;
    }
}

static void Stage_HandleCall(StageLuaState *st, StageLuaCall call, const char *args, const char *argsEnd)
{
    StageArgs a = { args, argsEnd };
    char tag[STAGE_TAG_MAX];

    if (call == LUA_CALL_GLITCH) {
        ++st->glitch;
        return;
    }
    if (call == LUA_CALL_IGNORED || st->ctx == CTX_RUNTIME) {
        ++st->ignored;
        return;
    }
    if (call == LUA_CALL_SET_PROPERTY) {
        if (st->ctx == CTX_SETUP)
            Stage_SetProperty(st, &a);
        else
            ++st->ignored;
        return;
    }
    if (!Stage_ArgString(st, &a, tag, sizeof(tag)) || tag[0] == '\0') {
        ++st->ignored;
        return;
    }
    if (call == LUA_CALL_PLAY_ANIM) {
        Stage_PlayAnim(st, Stage_FindSprite(st, tag), &a);
        return;
    }
    if (st->ctx != CTX_SETUP) {
        ++st->ignored; /* so playAnim vale em onBeatHit/onStepHit/... */
        return;
    }

    bool created = false;
    const int found = Stage_FindSprite(st, tag);
    StageSprite *sprite = found >= 0 ? &st->out[found] : NULL;

    switch (call) {
        case LUA_CALL_MAKE_SPRITE:
        case LUA_CALL_MAKE_ANIMATED_SPRITE: {
            /* makeLuaSprite(tag, imagem = '', x = 0, y = 0): refazer a tag recria o sprite. */
            char image[128] = {0};
            float x = 0.0f, y = 0.0f;
            (void)Stage_ArgString(st, &a, image, sizeof(image));
            (void)Stage_ArgFloat(st, &a, &x);
            (void)Stage_ArgFloat(st, &a, &y);

            const int index = Stage_GetOrCreateSprite(st, tag, &created);
            if (index < 0)
                break;
            if (!created) {
                Stage_ResetSprite(st, index);
                for (int i = 0; st->script != NULL && i < st->script->animCount; ++i) {
                    if (st->script->anims[i].sprite == index)
                        st->script->anims[i].sprite = UINT8_MAX;
                }
            }
            sprite = &st->out[index];
            snprintf(sprite->image, sizeof(sprite->image), "%s", image);
            sprite->x = x;
            sprite->y = y;
            sprite->isAnimated = call == LUA_CALL_MAKE_ANIMATED_SPRITE ? 1 : 0;
        } break;

        case LUA_CALL_MAKE_GRAPHIC: {
            /* makeGraphic(tag, w, h, cor = 'FFFFFF'). */
            float width = 0.0f, height = 0.0f;
            uint8_t rgba[4] = {255, 255, 255, 255};
            const char *s, *e;
            if (!Stage_ArgFloat(st, &a, &width) || !Stage_ArgFloat(st, &a, &height) ||
                width < 0.0f || height < 0.0f)
                break;
            if (Stage_NextArg(&a, &s, &e))
                (void)Stage_EvalColor(st, s, e, rgba);

            const int index = Stage_GetOrCreateSprite(st, tag, &created);
            if (index < 0)
                break;
            sprite = &st->out[index];
            sprite->image[0] = '\0';
            sprite->graphicWidth = width;
            sprite->graphicHeight = height;
            sprite->colorR = rgba[0];
            sprite->colorG = rgba[1];
            sprite->colorB = rgba[2];
            sprite->colorA = rgba[3];
            sprite->isGraphic = 1;
        } break;

        case LUA_CALL_LOAD_GRAPHIC: {
            char image[128];
            if (sprite != NULL && Stage_ArgString(st, &a, image, sizeof(image)) && image[0] != '\0') {
                snprintf(sprite->image, sizeof(sprite->image), "%s", image);
                sprite->isGraphic = 0;
            }
        } break;

        case LUA_CALL_SET_SCROLL:
        case LUA_CALL_SCALE_OBJECT:
        case LUA_CALL_SCALE_LUA: {
            float x = 1.0f, y = 1.0f;
            if (sprite == NULL || !Stage_ArgFloat(st, &a, &x))
                break;
            if (!Stage_ArgFloat(st, &a, &y))
                y = x;

            if (call == LUA_CALL_SET_SCROLL) {
                sprite->scrollX = x;
                sprite->scrollY = y;
            } else {
                /* scaleObject(tag, x, y, updateHitbox = true). */
                bool hitbox = true;
                if (call == LUA_CALL_SCALE_OBJECT)
                    (void)Stage_ArgBool(&a, &hitbox);
                sprite->scaleX = x;
                sprite->scaleY = y;
                sprite->scaleCenter = hitbox ? 0 : 1;
                sprite->sizeW = sprite->sizeH = 0.0f;
            }
        } break;

        case LUA_CALL_GRAPHIC_SIZE: {
            /* setGraphicSize(tag, w, h = 0, updateHitbox = true). */
            float width = 0.0f, height = 0.0f;
            bool hitbox = true;
            if (sprite == NULL || !Stage_ArgFloat(st, &a, &width))
                break;
            (void)Stage_ArgFloat(st, &a, &height);
            (void)Stage_ArgBool(&a, &hitbox);
            sprite->sizeW = width > 0.0f ? width : 0.0f;
            sprite->sizeH = height > 0.0f ? height : 0.0f;
            sprite->scaleCenter = hitbox ? 0 : 1;
        } break;

        case LUA_CALL_UPDATE_HITBOX:
            if (sprite != NULL)
                sprite->scaleCenter = 0;
            break;

        case LUA_CALL_SCREEN_CENTER: {
            char axes[8] = "xy";
            if (sprite == NULL)
                break;
            (void)Stage_ArgString(st, &a, axes, sizeof(axes));
            const bool x = strchr(axes, 'x') != NULL || strchr(axes, 'X') != NULL;
            const bool y = strchr(axes, 'y') != NULL || strchr(axes, 'Y') != NULL;
            sprite->center |= (uint8_t)((x || !y ? 1 : 0) | (y || !x ? 2 : 0));
        } break;

        case LUA_CALL_ADD_SPRITE: {
            bool front = false;
            if (sprite == NULL)
                break;
            (void)Stage_ArgBool(&a, &front);
            sprite->front = front ? 1 : 0;
            st->addSeq[found] = ++st->addCounter;
        } break;

        case LUA_CALL_REMOVE_SPRITE:
            if (sprite != NULL) {
                sprite->image[0] = '\0';
                sprite->isGraphic = 0;
                st->addSeq[found] = 0;
            }
            break;

        case LUA_CALL_SET_OBJECT_ORDER: {
            float order = 0.0f;
            if (sprite != NULL && Stage_ArgFloat(st, &a, &order)) {
                /* Psych: o numero e o indice na lista do PlayState:
                   [fixos, sprites de fundo..., gf, dad, bf, frente...].
                   Numero baixo fica atras dos personagens; so passa para
                   a frente depois dos tres grupos (fundos + 4). */
                sprite->front = order >= (float)(st->objectCount + 3) ? 1 : 0;
            }
        } break;

        case LUA_CALL_ADD_ANIM_PREFIX:
        case LUA_CALL_ADD_ANIM_INDICES:
        case LUA_CALL_ADD_ANIM_INDICES_LOOP: {
            /* addAnimationByPrefix(tag, nome, prefixo, fps = 24, loop = true)
               addAnimationByIndices(tag, nome, prefixo, '0,1,2', fps = 24, loop = false) */
            char name[32], prefix[64], indices[128] = {0};
            float fps = 24.0f;
            bool loop = call != LUA_CALL_ADD_ANIM_INDICES;
            if (sprite == NULL || !Stage_ArgString(st, &a, name, sizeof(name)) ||
                !Stage_ArgString(st, &a, prefix, sizeof(prefix)))
                break;

            if (call != LUA_CALL_ADD_ANIM_PREFIX) {
                const char *s, *e;
                if (!Stage_NextArg(&a, &s, &e))
                    break;
                if (*s == '{' && e[-1] == '}') { /* tabela {0, 1, 2} */
                    const size_t length = (size_t)(e - s - 2);
                    if (length >= sizeof(indices))
                        break;
                    memcpy(indices, s + 1, length);
                    indices[length] = '\0';
                } else if (!Stage_EvalString(st, s, e, indices, sizeof(indices))) {
                    break;
                }
                if (indices[0] == '\0')
                    break;
            }
            (void)Stage_ArgFloat(st, &a, &fps);
            if (call != LUA_CALL_ADD_ANIM_INDICES_LOOP)
                (void)Stage_ArgBool(&a, &loop);
            Stage_AddAnim(st, found, name, prefix, indices, fps, loop);
        } break;

        case LUA_CALL_ADD_OFFSET: {
            char name[32];
            float x = 0.0f, y = 0.0f;
            if (sprite == NULL || !Stage_ArgString(st, &a, name, sizeof(name)))
                break;
            (void)Stage_ArgFloat(st, &a, &x);
            (void)Stage_ArgFloat(st, &a, &y);
            StageAnimDef *def = Stage_FindAnimDef(st, found, name);
            if (def != NULL) {
                def->offsetX = x;
                def->offsetY = y;
            }
        } break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Parse                                                                     */
/* ------------------------------------------------------------------------- */

static void Stage_Run(StageLuaState *st, const char *text)
{
    const char *p = text;

    while (*p != '\0') {
        if (isspace((unsigned char)*p)) {
            ++p;
            continue;
        }
        const char *skipped = Stage_SkipNonCode(p);
        if (skipped != p) {
            p = skipped;
            continue;
        }
        if (!isalpha((unsigned char)*p) && *p != '_') {
            ++p;
            continue;
        }

        const char *token = p++;
        while (Stage_IsIdent(*p))
            ++p;
        const size_t length = (size_t)(p - token);
        const bool member = token > text && (token[-1] == '.' || token[-1] == ':');

        if (!member) {
            if (Stage_TokenEquals(token, length, "function")) {
                p = Stage_EnterFunction(st, p);
                continue;
            }
            if (Stage_TokenEquals(token, length, "if") || Stage_TokenEquals(token, length, "elseif")) {
                p = Stage_EnterIf(st, p, token[0] == 'e');
                continue;
            }
            if (Stage_TokenEquals(token, length, "else")) {
                Stage_EnterElse(st);
                continue;
            }
            if (Stage_TokenEquals(token, length, "do") || Stage_TokenEquals(token, length, "repeat")) {
                (void)Stage_PushBlock(st, BLOCK_OTHER);
                continue;
            }
            if (Stage_TokenEquals(token, length, "end") || Stage_TokenEquals(token, length, "until")) {
                Stage_PopBlock(st);
                continue;
            }
        }

        const StageLuaCall call = Stage_GetCall(token, length);
        const char *next = p;
        while (*next == ' ' || *next == '\t')
            ++next;

        if (call == LUA_CALL_NONE) {
            /* "x = valor" (nao "=="): variavel simples, so fora das callbacks do jogo. */
            if (!member && st->ctx == CTX_SETUP && next[0] == '=' && next[1] != '=')
                Stage_Assign(st, token, length, next + 1, Stage_StatementEnd(next + 1));
            continue;
        }

        while (*next != '\0' && isspace((unsigned char)*next))
            ++next;
        if (*next != '(') {
            p = next;
            continue;
        }

        const char *afterCall = Stage_FindCallEnd(next);
        if (afterCall == NULL)
            break;

        Stage_HandleCall(st, call, next + 1, afterCall - 1);
        p = afterCall;
    }
}

/* Ordem de desenho = ordem do addLuaSprite (como na Psych). Sem nenhum addLuaSprite lido,
 * fica a ordem de criacao e todos aparecem. Remapeia animacoes e acoes. */
static int Stage_Finish(StageLuaState *st)
{
    StageSprite *out = st->out;
    int order[STAGE_MAX_SPRITES];
    int keys[STAGE_MAX_SPRITES];
    int count = 0;

    for (int i = 0; i < st->objectCount; ++i) {
        if (out[i].image[0] == '\0' && !out[i].isGraphic)
            continue;
        if (st->addCounter > 0 && st->addSeq[i] == 0)
            continue; /* nunca adicionado no create (ex.: so num onEvent) */

        const int key = st->addCounter > 0 ? st->addSeq[i] : i;
        int at = count++;
        while (at > 0 && keys[at - 1] > key) {
            keys[at] = keys[at - 1];
            order[at] = order[at - 1];
            --at;
        }
        keys[at] = key;
        order[at] = i;
    }

    int remap[STAGE_MAX_SPRITES];
    for (int i = 0; i < STAGE_MAX_SPRITES; ++i)
        remap[i] = -1;

    StageSprite sorted[STAGE_MAX_SPRITES];
    for (int i = 0; i < count; ++i) {
        sorted[i] = out[order[i]];
        remap[order[i]] = i;
    }
    if (count > 0)
        memcpy(out, sorted, sizeof(StageSprite) * (size_t)count);

    StageScript *script = st->script;
    if (script != NULL) {
        int kept = 0;
        for (int i = 0; i < script->animCount; ++i) {
            StageAnimDef def = script->anims[i];
            if (def.sprite >= STAGE_MAX_SPRITES || remap[def.sprite] < 0)
                continue;
            def.sprite = (uint8_t)remap[def.sprite];
            script->anims[kept++] = def;
        }
        script->animCount = kept;

        kept = 0;
        for (int i = 0; i < script->actionCount; ++i) {
            StageAnimAction action = script->actions[i];
            if (action.sprite >= STAGE_MAX_SPRITES || remap[action.sprite] < 0)
                continue;
            action.sprite = (uint8_t)remap[action.sprite];
            script->actions[kept++] = action;
        }
        script->actionCount = kept;
    }
    return count;
}

static int Stage_Parse(const char *luaText, StageSprite *out, int capacity, StageScript *script,
                       int *ignoredGlitch, int *ignored)
{
    if (script != NULL)
        memset(script, 0, sizeof(*script));
    *ignoredGlitch = *ignored = 0;

    if (luaText == NULL || out == NULL || capacity <= 0)
        return 0;

    StageLuaState *st = calloc(1, sizeof(StageLuaState));
    if (st == NULL)
        return 0;

    st->out = out;
    st->capacity = capacity > STAGE_MAX_SPRITES ? STAGE_MAX_SPRITES : capacity;
    st->script = script;
    st->ctx = CTX_SETUP;

    Stage_Run(st, luaText);
    const int count = Stage_Finish(st);

    *ignoredGlitch = st->glitch;
    *ignored = st->ignored;
    if (script != NULL) {
        script->ignoredGlitch = st->glitch;
        script->ignored = st->ignored;
    }
    free(st);
    return count;
}

int Stage_ParseLua(const char *luaText, StageSprite *out, int capacity, int *ignoredGlitch, int *ignoredAnim)
{
    int glitch = 0, ignored = 0;
    const int count = Stage_Parse(luaText, out, capacity, NULL, &glitch, &ignored);
    if (ignoredGlitch != NULL) *ignoredGlitch = glitch;
    if (ignoredAnim != NULL) *ignoredAnim = ignored;
    return count;
}

int Stage_ParseLuaScript(const char *luaText, StageSprite *out, int capacity, StageScript *script)
{
    int glitch = 0, ignored = 0;
    return Stage_Parse(luaText, out, capacity, script, &glitch, &ignored);
}
