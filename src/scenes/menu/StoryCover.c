/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * StoryCover.c - busca e carrega a capa da week selecionada.
 *
 * A chave da capa vem, em ordem: do campo weekBackground do JSON da week, do
 * personagem da primeira musica e do titulo. Cai na imagem padrao se nada existir.
 * So olha o mod dono da week e os assets base, sem listar pastas.
 */
#include "Render.h"
#include "scenes/menu/StoryCover.h"
#include "yyjson.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scenes/Week.h"

#define PATH_MAX_LEN 512
#define KEY_MAX_LEN 64
#define MAX_WEEK_JSON_SIZE (256 * 1024)
#define DEFAULT_COVER "assets/images/freeplay/menuBGBlue.png"

typedef struct {
    char attempted;
    char path[PATH_MAX_LEN];
} CoverPathCache;

static CoverPathCache* pathCache; // um por week: guarda sucesso e falha
static Texture2D cover;
static char coverLoaded;
static int coverIndex = -1;

/* ------------------------------------------------------------------ */
/* Texto                                                               */
/* ------------------------------------------------------------------ */

static int IsSafeAssetName(const char* name) {
    if (!name || !name[0] || name[0] == '.') return 0;
    return !strstr(name, "..") && !strchr(name, '/') && !strchr(name, '\\');
}

static int StartsWithNoCase(const char* text, const char* prefix) {
    while (*prefix) {
        const unsigned char a = (unsigned char)*text++;
        const unsigned char b = (unsigned char)*prefix++;
        if (!a || tolower(a) != tolower(b)) return 0;
    }
    return 1;
}

/* Procura `needle` em `text` ignorando maiusculas. */
static const char* FindNoCase(const char* text, const char* needle) {
    const size_t length = strlen(needle);

    for (const char* p = text; *p; ++p) {
        size_t i = 0;
        while (i < length && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == length) return p;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Pasta do mod                                                        */
/* ------------------------------------------------------------------ */

/*
 * Extrai a pasta do mod a partir do diretorio da week. Aceita:
 *   assets/mods/<mod>/weeks[/<arquivo>]   assets/mods/<mod>/songs/<musica>
 *   assets/mods/mods/<mod>/... (layout antigo)   <jogo>/mods/<mod>   caminhos absolutos
 */
static int GetModRoot(const char* weekDir, char out[PATH_MAX_LEN]) {
    char normalized[PATH_MAX_LEN];

    out[0] = '\0';
    if (!weekDir || !weekDir[0]) return 0;

    const size_t length = strlen(weekDir);
    if (length >= sizeof(normalized)) return 0;
    for (size_t i = 0; i <= length; ++i)
        normalized[i] = weekDir[i] == '\\' ? '/' : weekDir[i];

    const char* modStart;
    const char* marker = FindNoCase(normalized, "assets/mods/");
    if (marker) {
        modStart = marker + sizeof("assets/mods/") - 1;
    } else if ((marker = FindNoCase(normalized, "/mods/")) != NULL) {
        modStart = marker + sizeof("/mods/") - 1; // <jogo>/mods/<mod>
    } else {
        return 0;
    }

    const char* modEnd = strchr(modStart, '/');
    if (!modEnd || modEnd == modStart) return 0;

    // Imports antigos usavam assets/mods/mods/<mod>/.
    if ((size_t)(modEnd - modStart) == 4 && StartsWithNoCase(modStart, "mods")) {
        const char* legacyStart = modEnd + 1;
        const char* legacyEnd = strchr(legacyStart, '/');
        if (!legacyEnd || legacyEnd == legacyStart) return 0;
        modEnd = legacyEnd;
    }

    const size_t rootLength = (size_t)(modEnd - normalized);
    if (rootLength == 0 || rootLength >= PATH_MAX_LEN) return 0;
    memcpy(out, normalized, rootLength);
    out[rootLength] = '\0';
    return 1;
}

/* ------------------------------------------------------------------ */
/* Chaves da capa                                                      */
/* ------------------------------------------------------------------ */

/* Aceita "charlie", "menu_charlie", "menu_charlie.png" ou um caminho; devolve "charlie". */
static int NormalizeCoverKey(const char* input, char out[KEY_MAX_LEN]) {
    out[0] = '\0';
    if (!input || !input[0]) return 0;

    const char* base = input;
    const char* slash = strrchr(base, '/');
    const char* backslash = strrchr(base, '\\');
    if (slash && (!backslash || slash > backslash)) base = slash + 1;
    else if (backslash) base = backslash + 1;

    size_t length = strlen(base);
    if (length >= 4 && base[length - 4] == '.' &&
        (base[length - 3] | 0x20) == 'p' && (base[length - 2] | 0x20) == 'n' && (base[length - 1] | 0x20) == 'g')
        length -= 4;

    if (length >= 5 && StartsWithNoCase(base, "menu_")) {
        base += 5;
        length -= 5;
    }

    if (length == 0 || length >= KEY_MAX_LEN) return 0;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)base[i];
        if (c < 32 || c == ':' || c == '/' || c == '\\') return 0;
    }

    memcpy(out, base, length);
    out[length] = '\0';
    return IsSafeAssetName(out);
}

/* "week charlie" -> "charlie" (minusculo, separadores viram '_'). */
static int MakeTitleKey(const char* title, char out[KEY_MAX_LEN]) {
    size_t used = 0;
    out[0] = '\0';
    if (!title) return 0;

    const unsigned char* source = (const unsigned char*)title;
    while (*source && isspace(*source)) ++source;
    if (StartsWithNoCase((const char*)source, "week ")) source += 5;

    while (*source && used + 1 < KEY_MAX_LEN) {
        const unsigned char c = *source++;
        if (isalnum(c))
            out[used++] = (char)tolower(c);
        else if ((c == ' ' || c == '-' || c == '_') && used > 0 && out[used - 1] != '_')
            out[used++] = '_';
    }

    while (used > 0 && out[used - 1] == '_') --used;
    out[used] = '\0';
    return used > 0;
}

/* Le "weekBackground" de <weekDir>/week-<chave>.json (formato Psych). Um unico arquivo. */
static int ReadWeekBackgroundKey(const char* weekDir, const char* titleKey, char out[KEY_MAX_LEN]) {
    out[0] = '\0';
    if (!weekDir || !weekDir[0] || !titleKey || !titleKey[0]) return 0;

    char path[PATH_MAX_LEN];
    const int n = snprintf(path, sizeof(path), "%s/week-%s.json", weekDir, titleKey);
    if (n <= 0 || (size_t)n >= sizeof(path)) return 0;

    FILE* file = fopen(path, "rb");
    if (!file) return 0;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    const long size = ftell(file);
    if (size <= 0 || size > MAX_WEEK_JSON_SIZE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    char* buffer = (char*)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(file);
        return 0;
    }

    const size_t readSize = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (readSize != (size_t)size) {
        free(buffer);
        return 0;
    }
    buffer[size] = '\0';

    yyjson_doc* doc = yyjson_read(buffer, (size_t)size, 0);
    free(buffer);
    if (!doc) return 0;

    yyjson_val* value = yyjson_obj_get(yyjson_doc_get_root(doc), "weekBackground");
    const char* key = yyjson_is_str(value) ? yyjson_get_str(value) : NULL;
    const int found = key && NormalizeCoverKey(key, out);

    yyjson_doc_free(doc);
    return found;
}

/* ------------------------------------------------------------------ */
/* Caminho da capa                                                     */
/* ------------------------------------------------------------------ */

static int PathFits(int n) { return n > 0 && (size_t)n < PATH_MAX_LEN; }

/* Testa a capa no mod dono da week e depois nos assets base. */
static int TryCoverPath(const char* weekDir, const char* key, char out[PATH_MAX_LEN]) {
    char modRoot[PATH_MAX_LEN];

    if (!IsSafeAssetName(key)) return 0;

    if (GetModRoot(weekDir, modRoot)) {
        const int n = snprintf(out, PATH_MAX_LEN, "%s/images/menubackgrounds/menu_%s.png", modRoot, key);
        if (PathFits(n) && FileExists(out)) return 1;
    }

    const int n = snprintf(out, PATH_MAX_LEN, "assets/images/menubackgrounds/menu_%s.png", key);
    return PathFits(n) && FileExists(out);
}

static int FindCoverPath(const WeekEntry* week, char out[PATH_MAX_LEN]) {
    char titleKey[KEY_MAX_LEN] = {0};
    char backgroundKey[KEY_MAX_LEN] = {0};

    out[0] = '\0';
    MakeTitleKey(week->title, titleKey);

    // 1) weekBackground explicito no JSON da week.
    if (titleKey[0] && ReadWeekBackgroundKey(week->dir, titleKey, backgroundKey) &&
        TryCoverPath(week->dir, backgroundKey, out))
        return 1;

    // 2) personagem da primeira musica.
    if (week->songCount > 0 && week->songs[0].character[0]) {
        const char* characterKey = week->songs[0].character;
        if (StartsWithNoCase(characterKey, "icon-")) characterKey += 5;
        if (TryCoverPath(week->dir, characterKey, out)) return 1;
    }

    // 3) titulo da week.
    if (titleKey[0] && TryCoverPath(week->dir, titleKey, out)) return 1;

    // 4) imagem padrao.
    if (FileExists(DEFAULT_COVER)) {
        snprintf(out, PATH_MAX_LEN, "%s", DEFAULT_COVER);
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void StoryCover_Unload(void) {
    if (coverLoaded && cover.id != 0)
        UnloadTexture(cover);

    cover = (Texture2D){0};
    coverLoaded = 0;
    coverIndex = -1;
}

void StoryCover_Free(void) {
    StoryCover_Unload();
    free(pathCache);
    pathCache = NULL;
}

void StoryCover_Reset(int weekCount) {
    StoryCover_Free();
    if (weekCount > 0)
        pathCache = (CoverPathCache*)calloc((size_t)weekCount, sizeof(*pathCache));
}

void StoryCover_Load(int index, const WeekEntry* week) {
    char path[PATH_MAX_LEN];

    if (!week || coverIndex == index) return;

    StoryCover_Unload();
    coverIndex = index; // sucesso e falha ficam em cache enquanto a lista existir

    if (pathCache) {
        CoverPathCache* cache = pathCache + index;
        if (!cache->attempted) {
            cache->attempted = 1;
            (void)FindCoverPath(week, cache->path);
        }
        if (!cache->path[0]) return;
        snprintf(path, sizeof(path), "%s", cache->path);
    }
    else if (!FindCoverPath(week, path)) {
        return; // sem memoria para o cache: uma tentativa por selecao
    }

    cover = Render_LoadTexture(path);
    coverLoaded = cover.id != 0;
}

char StoryCover_IsLoaded(void) { return coverLoaded; }
Texture2D StoryCover_Texture(void) { return cover; }
