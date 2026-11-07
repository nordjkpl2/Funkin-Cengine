/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayIcon.c - busca do icone da musica selecionada no Freeplay.
 *
 * Ordem de preferencia: icone da week > healthicon do personagem > nome do oponente.
 * Pastas de mods vem antes das do jogo base. Roda so quando a selecao muda.
 */
#include "Render.h"
#include "scenes/menu/FreeplayIcon.h"
#include "yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PATH_MAX_LEN 512
#define NAME_MAX_LEN 96
#define MAX_ICON_DIRS 7
#define MAX_JSON_SIZE (1024L * 1024L)

static Texture2D icon;
static bool iconLoaded;
static char opponentName[NAME_MAX_LEN];

/* ------------------------------------------------------------------ */
/* Utilitarios de string                                               */
/* ------------------------------------------------------------------ */

static void CopyString(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t length = strlen(src);
    if (length >= dstSize) length = dstSize - 1;
    memcpy(dst, src, length);
    dst[length] = '\0';
}

/* Nome de asset sem "..", separadores ou caracteres de controle. */
static bool IsSafeAssetName(const char* name) {
    if (!name || !name[0] || strstr(name, "..")) return false;

    for (const unsigned char* p = (const unsigned char*)name; *p; ++p)
        if (*p == '/' || *p == '\\' || *p < 32) return false;

    return true;
}

/* Copia o nome trocando espacos/_/- pelo separador (ou removendo, se '\0'). */
static void NormalizeIconName(const char* source, char* destination, size_t destinationSize, char separator) {
    size_t write = 0;
    bool previousSeparator = false;

    for (size_t i = 0; source[i] && write + 1 < destinationSize; ++i) {
        const unsigned char c = (unsigned char)source[i];
        if (c == ' ' || c == '_' || c == '-') {
            if (separator != '\0' && !previousSeparator)
                destination[write++] = separator;
            previousSeparator = true;
        } else {
            destination[write++] = (char)c;
            previousSeparator = false;
        }
    }
    destination[write] = '\0';
}

/* ------------------------------------------------------------------ */
/* Pasta do mod e healthicon do personagem                             */
/* ------------------------------------------------------------------ */

/*
 * Extrai assets/mods/<mod> do caminho da musica. Aceita tambem o formato antigo
 * assets/mods/mods/<mod>/... e o mod na raiz (assets/mods/songs/<musica>).
 * O resultado usa '/', que o Windows aceita.
 */
static bool GetModRoot(const char* songDir, char* out, size_t outSize) {
    out[0] = '\0';
    if (!songDir || !songDir[0]) return false;

    char normalized[PATH_MAX_LEN];
    CopyString(normalized, sizeof(normalized), songDir);
    for (char* p = normalized; *p; ++p)
        if (*p == '\\') *p = '/';

    static const char marker[] = "assets/mods/";
    char* mods = strstr(normalized, marker);
    if (!mods) return false;

    char* modName = mods + sizeof(marker) - 1;
    char* separator = strchr(modName, '/');
    if (!separator) return false;

    // Mod na raiz: assets/mods/songs/<musica> usa assets/mods.
    if ((size_t)(separator - modName) == 5 && memcmp(modName, "songs", 5) == 0) {
        const size_t length = (size_t)(modName - normalized);
        if (length == 0 || length >= outSize) return false;
        memcpy(out, normalized, length);
        out[length] = '\0';
        return true;
    }

    // Raiz duplicada antiga: mods/mods/<nome>.
    if ((size_t)(separator - modName) == 4 && memcmp(modName, "mods", 4) == 0) {
        modName = separator + 1;
        separator = strchr(modName, '/');
        if (!separator) return false;
    }

    if (separator == modName) return false;

    const size_t length = (size_t)(separator - normalized);
    if (length == 0 || length >= outSize) return false;

    memcpy(out, normalized, length);
    out[length] = '\0';
    return true;
}

/* Le o campo "healthicon" do JSON do personagem. */
static bool ReadCharacterHealthIcon(const char* jsonPath, char out[NAME_MAX_LEN]) {
    out[0] = '\0';

    FILE* file = fopen(jsonPath, "rb");
    if (!file) return false;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    const long fileSize = ftell(file);
    if (fileSize <= 0 || fileSize > MAX_JSON_SIZE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    char* buffer = (char*)malloc((size_t)fileSize + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }

    const size_t readSize = fread(buffer, 1, (size_t)fileSize, file);
    fclose(file);

    if (readSize != (size_t)fileSize) {
        free(buffer);
        return false;
    }
    buffer[fileSize] = '\0';

    yyjson_doc* doc = yyjson_read(buffer, readSize, 0);
    free(buffer);
    if (!doc) return false;

    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* value = yyjson_is_obj(root) ? yyjson_obj_get(root, "healthicon") : NULL;

    bool found = false;
    if (yyjson_is_str(value)) {
        const char* healthIcon = yyjson_get_str(value);
        if (IsSafeAssetName(healthIcon)) {
            CopyString(out, NAME_MAX_LEN, healthIcon);
            found = out[0] != '\0';
        }
    }

    yyjson_doc_free(doc);
    return found;
}

/* Procura <personagem>.json nas pastas conhecidas e le o healthicon. */
static bool FindHealthIconKey(const char* songDir, const char* modRoot,
                              const char* opponent, char out[NAME_MAX_LEN]) {
    CopyString(out, NAME_MAX_LEN, opponent);
    if (!IsSafeAssetName(opponent)) return false;

    char directories[8][PATH_MAX_LEN];
    int count = 0;

    if (modRoot[0]) {
        snprintf(directories[count++], PATH_MAX_LEN, "%s/characters", modRoot);
        snprintf(directories[count++], PATH_MAX_LEN, "%s/images/characters", modRoot);
    }
    if (songDir && songDir[0])
        snprintf(directories[count++], PATH_MAX_LEN, "%s/characters", songDir);

    // Raizes compartilhadas antigas e jogo base.
    static const char* const fixed[] = {
        "assets/mods/mods/characters",
        "assets/mods/mods/images/characters",
        "assets/mods/characters",
        "assets/characters",
        "assets/images/characters",
    };
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
        CopyString(directories[count++], PATH_MAX_LEN, fixed[i]);

    char candidate[PATH_MAX_LEN];
    for (int i = 0; i < count; i++) {
        snprintf(candidate, sizeof(candidate), "%s/%s.json", directories[i], opponent);
        if (FileExists(candidate) && ReadCharacterHealthIcon(candidate, out)) return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Caminho do PNG do icone                                             */
/* ------------------------------------------------------------------ */

/* Testa <dir>/icon-<nome>.png, <dir>/<nome>.png e (se vier "icon-x") <dir>/x.png. */
static bool TryIconNameInDirectory(const char* directory, const char* name, char out[PATH_MAX_LEN]) {
    if (!directory[0] || !IsSafeAssetName(name)) return false;

    char stem[NAME_MAX_LEN];
    CopyString(stem, sizeof(stem), name);

    // Tira a extensao .png, se vier no nome.
    const size_t length = strlen(stem);
    if (length >= 4 && stem[length - 4] == '.' &&
        (stem[length - 3] | 0x20) == 'p' && (stem[length - 2] | 0x20) == 'n' && (stem[length - 1] | 0x20) == 'g')
        stem[length - 4] = '\0';
    if (!stem[0]) return false;

    const bool hasPrefix = strncmp(stem, "icon-", 5) == 0;
    char path[PATH_MAX_LEN];

    if (!hasPrefix) {
        snprintf(path, sizeof(path), "%s/icon-%s.png", directory, stem);
        if (FileExists(path)) {
            CopyString(out, PATH_MAX_LEN, path);
            return true;
        }
    }

    snprintf(path, sizeof(path), "%s/%s.png", directory, stem);
    if (FileExists(path)) {
        CopyString(out, PATH_MAX_LEN, path);
        return true;
    }

    // Weeks dizem "icon-charlie", mas importadores salvam como "charlie.png".
    if (hasPrefix && stem[5] != '\0') {
        snprintf(path, sizeof(path), "%s/%s.png", directory, stem + 5);
        if (FileExists(path)) {
            CopyString(out, PATH_MAX_LEN, path);
            return true;
        }
    }

    return false;
}

/* Tenta o nome como veio e com separadores trocados (-, _ ou nenhum), sem repetir. */
static bool TryIconNameVariants(const char* directory, const char* name, char out[PATH_MAX_LEN]) {
    char variants[4][NAME_MAX_LEN];
    CopyString(variants[0], sizeof(variants[0]), name);
    NormalizeIconName(name, variants[1], sizeof(variants[1]), '-');
    NormalizeIconName(name, variants[2], sizeof(variants[2]), '_');
    NormalizeIconName(name, variants[3], sizeof(variants[3]), '\0');

    for (int i = 0; i < 4; ++i) {
        if (!variants[i][0]) continue;

        bool duplicate = false;
        for (int j = 0; j < i && !duplicate; ++j)
            duplicate = strcmp(variants[i], variants[j]) == 0;
        if (duplicate) continue;

        if (TryIconNameInDirectory(directory, variants[i], out)) return true;
    }
    return false;
}

/* Tenta healthIcon e depois opponent (se for outro nome) em uma pasta. */
static bool TryNamesInDirectory(const char* directory, const char* healthIcon,
                                const char* opponent, char out[PATH_MAX_LEN]) {
    if (healthIcon && healthIcon[0] && TryIconNameVariants(directory, healthIcon, out))
        return true;

    if (opponent && opponent[0] && !(healthIcon && strcmp(healthIcon, opponent) == 0) &&
        TryIconNameVariants(directory, opponent, out))
        return true;

    return false;
}

static bool FindIconPath(const char* songDir, const char* modRoot, const char* healthIcon,
                         const char* opponent, char out[PATH_MAX_LEN]) {
    char directories[MAX_ICON_DIRS][PATH_MAX_LEN];
    int count = 0;

    // O mod ativo tem prioridade: assets/mods/<mod>/images/icons/.
    if (modRoot[0]) {
        snprintf(directories[count++], PATH_MAX_LEN, "%s/images/icons", modRoot);
        snprintf(directories[count++], PATH_MAX_LEN, "%s/icons", modRoot);
    }

    // Pastas compartilhadas antigas.
    CopyString(directories[count++], PATH_MAX_LEN, "assets/mods/mods/images/icons");
    CopyString(directories[count++], PATH_MAX_LEN, "assets/mods/images/icons");

    // Alguns packs deixam icones ao lado da musica.
    if (songDir && songDir[0])
        snprintf(directories[count++], PATH_MAX_LEN, "%s/images/icons", songDir);

    for (int i = 0; i < count; ++i)
        if (TryNamesInDirectory(directories[i], healthIcon, opponent, out)) return true;

    // Icones do jogo base so entram por ultimo.
    return TryNamesInDirectory("assets/images/icons", healthIcon, opponent, out);
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void FreeplayIcon_Unload(void) {
    if (iconLoaded && icon.id != 0)
        UnloadTexture(icon);

    icon = (Texture2D){0};
    iconLoaded = false;
    opponentName[0] = '\0';
}

/* Resolve e carrega o icone da musica sem tocar no icone unico do Freeplay.
 * Devolve textura com id 0 se nada foi achado. Quem chama descarrega. */
static Texture2D ResolveIcon(SongEntry* song, char opponent[NAME_MAX_LEN]) {
    Texture2D result = {0};
    char modRoot[PATH_MAX_LEN] = {0};
    (void)GetModRoot(song->dir, modRoot, sizeof(modRoot));

    // O SongList ja leu o chart no scan: nao le data.json de novo (player2 pode estar generico).
    opponent[0] = '\0';
    if (song->opp[0] && IsSafeAssetName(song->opp)) {
        CopyString(opponent, NAME_MAX_LEN, song->opp);
    } else if (SongList_Opponent(song->dir, opponent) && IsSafeAssetName(opponent)) {
        CopyString(song->opp, sizeof(song->opp), opponent);
    }
    if (!opponent[0]) CopyString(opponent, NAME_MAX_LEN, "unknown");

    char iconPath[PATH_MAX_LEN];

    // O icone da week (["musica", "icon-charlie", ...]) vence o player2 do chart,
    // pois alguns charts de mod ainda declaram player2="dad".
    if (song->weekIcon[0] && IsSafeAssetName(song->weekIcon) &&
        FindIconPath(song->dir, modRoot, song->weekIcon, NULL, iconPath))
        result = Render_LoadTexture(iconPath);

    // Depois, o healthicon do personagem.
    if (result.id == 0) {
        char healthIcon[NAME_MAX_LEN] = {0};
        (void)FindHealthIconKey(song->dir, modRoot, opponent, healthIcon);
        if (!healthIcon[0]) CopyString(healthIcon, sizeof(healthIcon), opponent);

        if (FindIconPath(song->dir, modRoot, healthIcon, opponent, iconPath))
            result = Render_LoadTexture(iconPath);
    }

    // Ultimo recurso: icone do BF.
    if (result.id == 0 && FileExists("assets/images/icons/bf.png"))
        result = Render_LoadTexture("assets/images/icons/bf.png");

    return result;
}

void FreeplayIcon_Load(SongEntry* song) {
    FreeplayIcon_Unload();
    if (!song) return;

    icon = ResolveIcon(song, opponentName);
    iconLoaded = icon.id != 0;
}

Texture2D FreeplayIcon_Resolve(SongEntry* song) {
    char opponent[NAME_MAX_LEN];
    return song ? ResolveIcon(song, opponent) : (Texture2D){0};
}

char FreeplayIcon_IsLoaded(void) { return iconLoaded; }
Texture2D FreeplayIcon_Texture(void) { return icon; }
const char* FreeplayIcon_Opponent(void) { return opponentName[0] ? opponentName : "unknown"; }
