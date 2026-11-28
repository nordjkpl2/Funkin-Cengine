/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NoteTypes.c - registro de tipos de nota e leitor do subset Lua da Psych (custom_notetypes).
 */
#include "logic/notes/NoteTypes.h"
#include "core/Log.h"
#include "core/ModList.h"
#include "core/ModPath.h"
#include "raylib.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NT_PATH_MAX 512
#define NT_STACK_MAX 32

static NoteTypeInfo types[NOTETYPE_MAX];
static atomic_int typeCount;                 /* 0 = ainda sem os tipos padrao */
static atomic_flag typeLock = ATOMIC_FLAG_INIT;

static const char* const builtinNames[NOTETYPE_BUILTIN] = {
    "", "Alt Animation", "Hey!", "Hurt Note", "GF Sing", "No Animation"
};

static void Lock(void) {
    while (atomic_flag_test_and_set_explicit(&typeLock, memory_order_acquire)) {}
}

static void Unlock(void) {
    atomic_flag_clear_explicit(&typeLock, memory_order_release);
}

/* Regras de nota normal (o nome fica). */
static void ResetRules(NoteTypeInfo* t) {
    char name[NOTETYPE_NAME_MAX];
    memcpy(name, t->name, sizeof(name));
    memset(t, 0, sizeof(*t));
    memcpy(t->name, name, sizeof(name));
    t->hitHealth = NAN;
    t->missHealth = NAN;
    t->hitAnimTarget = t->missAnimTarget = -1;
}

/* Valores da Psych 0.6 (vida em 0..1 aqui = metade da Psych). */
static void SetBuiltin(int i) {
    NoteTypeInfo* t = &types[i];
    snprintf(t->name, sizeof(t->name), "%s", builtinNames[i]);
    ResetRules(t);
    switch (i) {
    case NOTETYPE_ALT:
        snprintf(t->animSuffix, sizeof(t->animSuffix), "-alt");
        break;
    case NOTETYPE_HEY:
        t->hey = 1;
        break;
    case NOTETYPE_HURT:
        snprintf(t->texture, sizeof(t->texture), "HURTNOTE_assets");
        t->hitCausesMiss = t->ignorePlayer = t->lowPriority = 1;
        t->hurtAnim = t->hurtColor = t->noSplash = 1;
        t->missHealth = 0.15f;
        break;
    case NOTETYPE_GF:
        t->gfNote = 1;
        break;
    case NOTETYPE_NOANIM:
        t->noAnimation = t->noMissAnimation = 1;
        break;
    default:
        break;
    }
}

static int Count(void) {
    int n = atomic_load_explicit(&typeCount, memory_order_acquire);
    if (n != 0) return n;
    Lock();
    n = atomic_load_explicit(&typeCount, memory_order_relaxed);
    if (n == 0) {
        for (int i = 0; i < NOTETYPE_BUILTIN; i++) SetBuiltin(i);
        n = NOTETYPE_BUILTIN;
        atomic_store_explicit(&typeCount, n, memory_order_release);
    }
    Unlock();
    return n;
}

static int FindIn(const char* name, int from, int to) {
    for (int i = from; i < to; i++)
        if (strcmp(types[i].name, name) == 0) return i;
    return -1;
}

int NoteTypes_Find(const char* name) {
    if (name == NULL) return -1;
    return FindIn(name, 0, Count());
}

int NoteTypes_Intern(const char* name) {
    if (name == NULL || name[0] == 0) return NOTETYPE_NORMAL;
    int n = Count();
    int found = FindIn(name, 0, n);
    if (found >= 0) return found;

    Lock();
    const int now = atomic_load_explicit(&typeCount, memory_order_relaxed);
    found = FindIn(name, n, now);   /* outra thread pode ter criado */
    if (found < 0 && now < NOTETYPE_MAX) {
        NoteTypeInfo* t = &types[now];
        snprintf(t->name, sizeof(t->name), "%s", name);
        ResetRules(t);
        atomic_store_explicit(&typeCount, now + 1, memory_order_release);
        found = now;
    }
    Unlock();
    if (found < 0) {
        Log_WarnF("Tipos de nota: limite de %d atingido ('%s' vira nota normal)", NOTETYPE_MAX, name);
        return NOTETYPE_NORMAL;
    }
    return found;
}

int NoteTypes_FromNumber(double value) {
    if (!isfinite(value) || value < 0.0 || value >= NOTETYPE_BUILTIN) return NOTETYPE_NORMAL;
    const int i = (int)value;
    return (double)i == value ? i : NOTETYPE_NORMAL;
}

int NoteTypes_Count(void) { return Count(); }

const NoteTypeInfo* NoteTypes_Get(int type) {
    const int n = Count();
    return &types[(type >= 0 && type < n) ? type : NOTETYPE_NORMAL];
}

const char* NoteTypes_Name(int type) {
    return NoteTypes_Get(type)->name;
}

/* ------------------------------------------------------------------ */
/* Subset Lua: setPropertyFromGroup('unspawnNotes', i, 'prop', valor)  */
/* ------------------------------------------------------------------ */

enum { CTX_ANY = 0, CTX_PLAYER = 1, CTX_OPPONENT = 2 };
enum { FN_OTHER = 0, FN_HIT = 1, FN_MISS = 2 };

typedef struct {
    char kind;   /* CTX_*: if que testa mustPress */
    char fn;     /* FN_*: corpo de goodNoteHit / noteMiss */
} Block;

typedef struct {
    Block stack[NT_STACK_MAX];
    int depth;
    int pendingDo;   /* for/while ja abriram o bloco: o "do" deles nao abre outro */
} LuaState;

static int IsIdStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int IsIdChar(char c) { return IsIdStart(c) || (c >= '0' && c <= '9'); }

static const char* SkipWs(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

/* Comentario (-- ate o fim da linha, --[[ ... ]]) ou NULL se p nao e comentario. */
static const char* SkipComment(const char* p) {
    if (p[0] != '-' || p[1] != '-') return NULL;
    if (p[2] == '[' && p[3] == '[') {
        const char* e = strstr(p + 4, "]]");
        return e != NULL ? e + 2 : p + strlen(p);
    }
    while (*p && *p != '\n') p++;
    return p;
}

/* String Lua ('..' ou ".."). NULL se nao e string. out pode ser NULL (so pula). */
static const char* ReadString(const char* p, char* out, size_t cap) {
    const char q = *p;
    size_t n = 0;
    if (q != '\'' && q != '"') return NULL;
    p++;
    while (*p && *p != q && *p != '\n') {
        char c = *p++;
        if (c == '\\' && *p) c = *p++;
        if (out != NULL && n + 1 < cap) out[n++] = c;
    }
    if (out != NULL && cap > 0) out[n] = 0;
    return *p == q ? p + 1 : p;
}

static const char* SkipString(const char* p) {
    const char* e = ReadString(p, NULL, 0);
    return e != NULL ? e : p + 1;
}

/* Proximo argumento (depois da virgula, no nivel 0 dos parenteses). NULL = fim da chamada. */
static const char* NextArg(const char* p) {
    int depth = 0;
    while (*p) {
        if (*p == '\'' || *p == '"') { p = SkipString(p); continue; }
        if (*p == '(') depth++;
        else if (*p == ')') { if (depth == 0) return NULL; depth--; }
        else if (*p == ',' && depth == 0) return SkipWs(p + 1);
        else if (*p == '\n' && depth == 0) return NULL;
        p++;
    }
    return NULL;
}

static int Ctx(const LuaState* s) {
    for (int i = s->depth - 1; i >= 0; i--)
        if (s->stack[i].kind != CTX_ANY) return s->stack[i].kind;
    return CTX_ANY;
}

static int Fn(const LuaState* s) {
    for (int i = s->depth - 1; i >= 0; i--)
        if (s->stack[i].fn != FN_OTHER) return s->stack[i].fn;
    return FN_OTHER;
}

static void Push(LuaState* s, char kind, char fn) {
    if (s->depth < NT_STACK_MAX) s->stack[s->depth] = (Block){kind, fn};
    s->depth++;
}

static void Pop(LuaState* s) {
    if (s->depth > 0) s->depth--;
}

/* Condicao do if (ate o "then"): testa mustPress? Negada = lado do oponente. */
static char CondKind(const char* p) {
    const char* then = strstr(p, "then");
    const size_t len = then != NULL ? (size_t)(then - p) : strlen(p);
    char cond[256];
    const size_t n = len < sizeof(cond) - 1 ? len : sizeof(cond) - 1;
    memcpy(cond, p, n);
    cond[n] = 0;
    if (strstr(cond, "mustPress") == NULL) return CTX_ANY;
    const char neg = strstr(cond, "not ") != NULL || strstr(cond, "false") != NULL || strstr(cond, "~=") != NULL;
    return neg ? CTX_OPPONENT : CTX_PLAYER;
}

typedef struct {
    char str[64];
    double num;
    char isStr, isNum, isBool, b;
} LuaValue;

static void ReadValue(const char* p, LuaValue* v) {
    memset(v, 0, sizeof(*v));
    if (p == NULL) return;
    if (*p == '\'' || *p == '"') {
        ReadString(p, v->str, sizeof(v->str));
        v->isStr = 1;
        char* end = NULL;
        v->num = strtod(v->str, &end);
        v->isNum = end != v->str && isfinite(v->num);
        if (strcmp(v->str, "true") == 0 || strcmp(v->str, "false") == 0) {
            v->isBool = 1;
            v->b = v->str[0] == 't';
        }
        return;
    }
    if (strncmp(p, "true", 4) == 0 && !IsIdChar(p[4])) { v->isBool = 1; v->b = 1; return; }
    if (strncmp(p, "false", 5) == 0 && !IsIdChar(p[5])) { v->isBool = 1; return; }
    char* end = NULL;
    v->num = strtod(p, &end);
    v->isNum = end != p && isfinite(v->num);
}

static void ApplyProp(NoteTypeInfo* t, const char* prop, const LuaValue* v, int ctx) {
    const char b = v->isBool ? v->b : (v->isNum && v->num != 0.0);
    if (strcmp(prop, "texture") == 0) {
        if (v->isStr) snprintf(t->texture, sizeof(t->texture), "%s", v->str);
        t->hurtColor = 0;   /* textura propria: sem a cor de dano */
    } else if (strcmp(prop, "animSuffix") == 0) {
        if (v->isStr) snprintf(t->animSuffix, sizeof(t->animSuffix), "%s", v->str);
    } else if (strcmp(prop, "hitHealth") == 0) {
        if (v->isNum) t->hitHealth = (float)(v->num * 0.5);
    } else if (strcmp(prop, "missHealth") == 0) {
        if (v->isNum) t->missHealth = (float)(v->num * 0.5);
    } else if (strcmp(prop, "hitCausesMiss") == 0) {
        t->hitCausesMiss = b;
    } else if (strcmp(prop, "ignoreNote") == 0) {
        if (ctx != CTX_OPPONENT) t->ignorePlayer = b;
        if (ctx != CTX_PLAYER) t->ignoreOpponent = b;
    } else if (strcmp(prop, "noAnimation") == 0) {
        t->noAnimation = b;
    } else if (strcmp(prop, "noMissAnimation") == 0) {
        t->noMissAnimation = b;
    } else if (strcmp(prop, "gfNote") == 0) {
        t->gfNote = b;
    } else if (strcmp(prop, "lowPriority") == 0) {
        t->lowPriority = b;
    } else if (strcmp(prop, "noteSplashDisabled") == 0 || strcmp(prop, "noteSplashData.disabled") == 0) {
        t->noSplash = b;
    }
}

static signed char CharTarget(const char* name) {
    if (strcmp(name, "dad") == 0 || strcmp(name, "opponent") == 0) return 0;
    if (strcmp(name, "boyfriend") == 0 || strcmp(name, "bf") == 0) return 1;
    if (strcmp(name, "gf") == 0 || strcmp(name, "girlfriend") == 0) return 2;
    return -1;
}

/* Chamada reconhecida em p (logo apos o nome). Retorna onde continuar. */
static const char* ParseCall(NoteTypeInfo* t, const char* id, size_t idLen, const char* p, const LuaState* s) {
    p = SkipWs(p);
    if (*p != '(') return p;
    const char* a = SkipWs(p + 1);

    if (idLen == 20 && memcmp(id, "setPropertyFromGroup", 20) == 0) {
        char group[32], prop[48];
        LuaValue v;
        if (ReadString(a, group, sizeof(group)) == NULL) return a;
        if (strcmp(group, "unspawnNotes") != 0 && strcmp(group, "notes") != 0) return a;
        const char* a2 = NextArg(a);                 /* indice (i) */
        const char* a3 = a2 != NULL ? NextArg(a2) : NULL;
        if (a3 == NULL || ReadString(a3, prop, sizeof(prop)) == NULL) return a;
        ReadValue(NextArg(a3), &v);
        ApplyProp(t, prop, &v, Ctx(s));
        return a;
    }

    const int fn = Fn(s);
    if (fn == FN_OTHER) return a;

    if ((idLen == 17 && memcmp(id, "characterPlayAnim", 17) == 0) || (idLen == 8 && memcmp(id, "playAnim", 8) == 0)) {
        char who[32], anim[24];
        const char* a2 = ReadString(a, who, sizeof(who)) != NULL ? NextArg(a) : NULL;
        if (a2 == NULL || ReadString(a2, anim, sizeof(anim)) == NULL) return a;
        const signed char target = CharTarget(who);
        if (target < 0) return a;
        if (fn == FN_HIT) {
            snprintf(t->hitAnim, sizeof(t->hitAnim), "%s", anim);
            t->hitAnimTarget = target;
        } else {
            snprintf(t->missAnim, sizeof(t->missAnim), "%s", anim);
            t->missAnimTarget = target;
        }
    } else if (idLen == 11 && memcmp(id, "setProperty", 11) == 0) {
        char prop[32];
        LuaValue v;
        if (ReadString(a, prop, sizeof(prop)) == NULL || strcmp(prop, "health") != 0) return a;
        ReadValue(NextArg(a), &v);
        if (v.isNum && v.num <= 0.0) {
            if (fn == FN_HIT) t->hitKills = 1;
            else t->missKills = 1;
        }
    }
    return a;
}

static char FuncKind(const char* p) {
    p = SkipWs(p);
    if (strncmp(p, "goodNoteHit", 11) == 0 && !IsIdChar(p[11])) return FN_HIT;
    if (strncmp(p, "noteMiss", 8) == 0 && !IsIdChar(p[8])) return FN_MISS;
    return FN_OTHER;
}

static void ParseScript(NoteTypeInfo* t, const char* p) {
    LuaState s;
    memset(&s, 0, sizeof(s));
    while (*p) {
        const char* c = SkipComment(p);
        if (c != NULL) { p = c; continue; }
        if (*p == '\'' || *p == '"') { p = SkipString(p); continue; }
        if (p[0] == '[' && p[1] == '[') {   /* string longa */
            const char* e = strstr(p + 2, "]]");
            p = e != NULL ? e + 2 : p + strlen(p);
            continue;
        }
        if (!IsIdStart(*p)) { p++; continue; }

        const char* id = p;
        while (IsIdChar(*p)) p++;
        const size_t n = (size_t)(p - id);
#define KW(word) (n == sizeof(word) - 1 && memcmp(id, word, n) == 0)
        if (KW("if")) Push(&s, CondKind(p), FN_OTHER);
        else if (KW("elseif")) { if (s.depth > 0 && s.depth <= NT_STACK_MAX) s.stack[s.depth - 1].kind = CondKind(p); }
        else if (KW("else")) {
            if (s.depth > 0 && s.depth <= NT_STACK_MAX) {
                Block* top = &s.stack[s.depth - 1];
                if (top->kind != CTX_ANY) top->kind = top->kind == CTX_PLAYER ? CTX_OPPONENT : CTX_PLAYER;
            }
        }
        else if (KW("for") || KW("while")) { Push(&s, CTX_ANY, FN_OTHER); s.pendingDo++; }
        else if (KW("do")) { if (s.pendingDo > 0) s.pendingDo--; else Push(&s, CTX_ANY, FN_OTHER); }
        else if (KW("function")) Push(&s, CTX_ANY, FuncKind(p));
        else if (KW("repeat")) Push(&s, CTX_ANY, FN_OTHER);
        else if (KW("end") || KW("until")) Pop(&s);
        else p = ParseCall(t, id, n, p, &s);
#undef KW
    }
}

/* ------------------------------------------------------------------ */
/* Arquivos                                                            */
/* ------------------------------------------------------------------ */

static bool SafeName(const char* name) {
    return name[0] != 0 && strchr(name, '/') == NULL && strchr(name, '\\') == NULL &&
           strchr(name, ':') == NULL && strstr(name, "..") == NULL;
}

static bool FindScript(const char* modRoot, const char* name, char* out, size_t cap) {
    char rel[NT_PATH_MAX];
    if (!SafeName(name)) return false;
    if (snprintf(rel, sizeof(rel), "custom_notetypes/%s.lua", name) >= (int)sizeof(rel)) return false;
    if (modRoot[0] != 0 && ModPath_FindIn(modRoot, rel, out, cap)) return true;
    if (ModPath_FindIn("assets", rel, out, cap)) return true;
    for (int i = 0; i < ModList_Count(); i++)
        if (ModList_Enabled(i) && ModPath_FindIn(ModList_Root(i), rel, out, cap)) return true;
    return false;
}

void NoteTypes_LoadScripts(const char* songDir) {
    char modRoot[NT_PATH_MAX] = "";
    if (songDir != NULL && !ModPath_Root(songDir, modRoot, sizeof(modRoot))) modRoot[0] = 0;

    const int n = Count();
    for (int i = NOTETYPE_BUILTIN; i < n; i++) {
        NoteTypeInfo* t = &types[i];
        char path[NT_PATH_MAX];
        ResetRules(t);
        if (!FindScript(modRoot, t->name, path, sizeof(path))) continue;
        char* text = LoadFileText(path);
        if (text == NULL) continue;
        ParseScript(t, text);
        UnloadFileText(text);
        t->scripted = 1;
    }
}

static void ScanDir(const char* root) {
    char dir[NT_PATH_MAX];
    if (root[0] == 0 || snprintf(dir, sizeof(dir), "%s/custom_notetypes", root) >= (int)sizeof(dir)) return;
    if (!DirectoryExists(dir)) return;
    FilePathList list = LoadDirectoryFilesEx(dir, ".lua", false);
    for (unsigned int i = 0; i < list.count; i++)
        (void)NoteTypes_Intern(GetFileNameWithoutExt(list.paths[i]));
    UnloadDirectoryFiles(list);
}

void NoteTypes_ScanScripts(const char* songDir) {
    char modRoot[NT_PATH_MAX] = "";
    if (songDir != NULL && ModPath_Root(songDir, modRoot, sizeof(modRoot))) ScanDir(modRoot);
    ScanDir("assets");
}
