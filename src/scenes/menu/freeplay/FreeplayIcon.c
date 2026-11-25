/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayIcon.c - busca do icone da musica selecionada no Freeplay.
 *
 * Ordem de preferencia: icone da week > healthicon do personagem > nome do oponente.
 * Pastas de mods vem antes das do jogo base. Roda so quando a selecao muda.
 */
#include "Render.h"
#include "scenes/menu/freeplay/FreeplayIcon.h"
#include "yyjson.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "threads/cpu/Jobs.h"
#include "threads/Thread.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PATH_MAX_LEN 512
#define NAME_MAX_LEN 96
#define MAX_ICON_DIRS 7
#define MAX_JSON_SIZE (1024L * 1024L)

static const SongEntry* curSong;  /* musica do Freeplay (FreeplayIcon_Load) */
static bool busy;                 /* curSong ainda resolvendo ou decodificando */

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
/* Cache                                                               */
/* ------------------------------------------------------------------ */

/*
 * Memo: uma entrada por musica (indice = posicao em songs[]). Resolver o caminho le disco, entao roda
 * numa thread de fundo (Jobs); so depois de MEMO_DONE a thread principal le o resultado.
 * Texturas: LRU; o PNG ja vem decodificado do ImagePrefetch, so o envio para a GPU fica no frame.
 */
#define TEX_SLOTS 48        /* a lista do Chart Editor mostra ~20 icones; o LRU nunca despeja os visiveis */
#define WANT_MAX 64         /* vizinhos esperando a resolucao para pedir o PNG em fundo */
#define SYNC_PER_FRAME 1    /* fallback: no maximo uma decodificacao na thread principal por frame */

enum { MEMO_IDLE = 0, MEMO_BUSY = 1, MEMO_DONE = 2 };

typedef struct {
    char path[PATH_MAX_LEN];       /* vazio = nada achado; escrito pela thread de fundo ate MEMO_DONE */
    char opponent[NAME_MAX_LEN];
    const SongEntry* song;
    atomic_int state;              /* MEMO_*; a thread de fundo publica MEMO_DONE por ultimo */
    char failed;                   /* so thread principal: a textura nao carregou, nao tenta de novo */
} IconMemo;

typedef struct {
    char path[PATH_MAX_LEN];
    Texture2D texture;
    unsigned long long stamp;
    bool used;
} IconTexture;

static const SongEntry* memoBase;  /* songs[] de quem chamou Init */
static IconMemo* memo;
static int memoCount;
static IconTexture textures[TEX_SLOTS];
static unsigned long long useClock;
static int wants[WANT_MAX];        /* indices em memo que esperam a resolucao para pedir o PNG */
static int wantCount;
static int syncBudget;             /* decodificacoes sincronas ainda permitidas neste frame */

/* Caminho do icone (ou vazio) e oponente da musica. Le disco: so uma vez por musica. */
static void ResolvePath(const SongEntry* song, char opponent[NAME_MAX_LEN], char out[PATH_MAX_LEN]) {
    char modRoot[PATH_MAX_LEN] = {0};
    (void)GetModRoot(song->dir, modRoot, sizeof(modRoot));

    // O SongList ja leu o chart no scan: nao le data.json de novo (player2 pode estar generico).
    opponent[0] = '\0';
    if (song->opp[0] && IsSafeAssetName(song->opp)) {
        CopyString(opponent, NAME_MAX_LEN, song->opp);
    } else {
        (void)SongList_Opponent(song->dir, opponent);
    }
    if (!IsSafeAssetName(opponent)) opponent[0] = '\0';
    if (!opponent[0]) CopyString(opponent, NAME_MAX_LEN, "unknown");

    out[0] = '\0';

    // O icone da week vence o player2 do chart (alguns charts de mod ainda declaram "dad").
    if (song->weekIcon[0] && IsSafeAssetName(song->weekIcon))
        (void)FindIconPath(song->dir, modRoot, song->weekIcon, NULL, out);

    // Depois, o healthicon do personagem.
    if (out[0] == '\0') {
        char healthIcon[NAME_MAX_LEN] = {0};
        (void)FindHealthIconKey(song->dir, modRoot, opponent, healthIcon);
        if (!healthIcon[0]) CopyString(healthIcon, sizeof(healthIcon), opponent);
        (void)FindIconPath(song->dir, modRoot, healthIcon, opponent, out);
    }

    // Ultimo recurso: icone do BF.
    if (out[0] == '\0' && FileExists("assets/images/icons/bf.png"))
        CopyString(out, PATH_MAX_LEN, "assets/images/icons/bf.png");
}

/* Thread de fundo: so le disco (ResolvePath usa buffers locais e constantes). Publica DONE por ultimo. */
static void ResolveJob(void* data) {
    IconMemo* m = (IconMemo*)data;
    ResolvePath(m->song, m->opponent, m->path);
    atomic_store_explicit(&m->state, MEMO_DONE, memory_order_release);
}

/* Pede a resolucao da musica se ainda nao foi (nao bloqueia). NULL se a musica nao esta na lista. */
static IconMemo* MemoRequest(const SongEntry* song) {
    if (!memo || !song || song < memoBase || song >= memoBase + memoCount) return NULL;
    IconMemo* m = memo + (song - memoBase);
    if (atomic_load_explicit(&m->state, memory_order_acquire) == MEMO_IDLE) {
        atomic_store_explicit(&m->state, MEMO_BUSY, memory_order_relaxed);
        Jobs_Init(0);
        if (!Jobs_Submit(ResolveJob, m, NULL)) ResolveJob(m); /* fila recusou: resolve agora (raro) */
    }
    return m;
}

static bool MemoDone(const IconMemo* m) {
    return atomic_load_explicit(&m->state, memory_order_acquire) == MEMO_DONE;
}

/* Musica ainda sem resolucao: Tick pede o PNG em fundo quando ela ficar pronta. */
static void Want(int idx) {
    for (int i = 0; i < wantCount; ++i)
        if (wants[i] == idx) return;
    if (wantCount < WANT_MAX) wants[wantCount++] = idx;
}

static IconTexture* TextureSlot(const char* path) {
    for (int i = 0; i < TEX_SLOTS; ++i)
        if (textures[i].used && strcmp(textures[i].path, path) == 0) return textures + i;
    return NULL;
}

/* Guarda a textura no LRU, despejando a menos usada. Os visiveis neste frame tem stamp recente e nao caem. */
static void TextureStore(const char* path, Texture2D texture) {
    int victim = -1;
    for (int i = 0; i < TEX_SLOTS && victim < 0; ++i)
        if (!textures[i].used) victim = i;
    if (victim < 0) {
        victim = 0;
        for (int i = 1; i < TEX_SLOTS; ++i)
            if (textures[i].stamp < textures[victim].stamp) victim = i;
        UnloadTexture(textures[victim].texture);
    }
    IconTexture* t = textures + victim;
    CopyString(t->path, sizeof(t->path), path);
    t->texture = texture;
    t->stamp = ++useClock;
    t->used = true;
}

/* Textura sem bloquear: 1 = pronta (em *out), 0 = ainda decodificando, -1 = sem icone.
 * So sobe para a GPU o PNG ja decodificado em fundo. Sem prefetch (fila cheia, sem memoria), carrega
 * na hora, mas no maximo SYNC_PER_FRAME vezes por frame. */
static int TryTexture(IconMemo* m, Texture2D* out) {
    if (m->path[0] == '\0' || m->failed) return -1;
    IconTexture* hit = TextureSlot(m->path);
    if (hit != NULL) {
        hit->stamp = ++useClock;
        *out = hit->texture;
        return 1;
    }
    if (ImagePrefetch_Start(m->path)) {
        if (!ImagePrefetch_Ready(m->path)) return 0;
    } else {
        if (syncBudget <= 0) return 0;
        --syncBudget;
    }
    const Texture2D t = Render_LoadTexture(m->path);
    if (t.id == 0) {
        m->failed = 1;
        return -1;
    }
    TextureStore(m->path, t);
    *out = t;
    return 1;
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void FreeplayIcon_Init(const SongEntry* songs, int count) {
    FreeplayIcon_Unload();
    memoBase = songs;
    memoCount = count > 0 ? count : 0;
    memo = memoCount > 0 ? calloc((size_t)memoCount, sizeof(IconMemo)) : NULL;
    if (memo == NULL) {
        memoCount = 0;
        return;
    }
    for (int i = 0; i < memoCount; ++i) memo[i].song = songs + i;
}

/* Solta tudo: texturas, memo e musica atual. Espera as resolucoes em voo (elas apontam para o memo). */
void FreeplayIcon_Unload(void) {
    for (int i = 0; i < memoCount; ++i)
        while (atomic_load_explicit(&memo[i].state, memory_order_acquire) == MEMO_BUSY)
            Thread_SleepMs(0);

    for (int i = 0; i < TEX_SLOTS; ++i) {
        if (textures[i].used && textures[i].texture.id != 0)
            UnloadTexture(textures[i].texture);
    }
    memset(textures, 0, sizeof(textures));
    free(memo);
    memo = NULL;
    memoCount = 0;
    memoBase = NULL;
    useClock = 0;
    wantCount = 0;
    curSong = NULL;
    busy = false;
}

/* Troca a musica atual (Freeplay). Nao bloqueia: FreeplayIcon_Tick termina o carregamento. */
void FreeplayIcon_Load(const SongEntry* song) {
    curSong = song;
    busy = song != NULL;
}

/* Pede em fundo a resolucao e, quando pronta, a decodificacao de uma musica vizinha. Nao bloqueia. */
void FreeplayIcon_Warm(const SongEntry* song) {
    IconMemo* m = MemoRequest(song);
    if (m == NULL) return;
    if (!MemoDone(m))
        Want((int)(m - memo));
    else if (m->path[0] != '\0' && !m->failed)
        ImagePrefetch_Start(m->path);
}

/* Chamar 1x por frame na thread principal: reseta o orcamento de decodificacao sincrona, manda decodificar
 * os vizinhos que ficaram prontos e tenta subir a musica atual. true = ainda carregando (redesenhar). */
bool FreeplayIcon_Tick(void) {
    syncBudget = SYNC_PER_FRAME;

    int keep = 0;
    for (int i = 0; i < wantCount; ++i) {
        IconMemo* m = memo + wants[i];
        if (!MemoDone(m))
            wants[keep++] = wants[i];
        else if (m->path[0] != '\0' && !m->failed)
            ImagePrefetch_Start(m->path);
    }
    wantCount = keep;

    busy = false;
    if (curSong != NULL) {
        IconMemo* m = MemoRequest(curSong);
        if (m != NULL) {
            Texture2D t = {0};
            busy = !MemoDone(m) || TryTexture(m, &t) == 0;
        }
    }
    return busy;
}

/* Icone de qualquer musica da lista (Chart Editor). 1 = pronta (em *out), 0 = carregando, -1 = sem icone. */
int FreeplayIcon_Get(const SongEntry* song, Texture2D* out) {
    IconMemo* m = MemoRequest(song);
    if (m == NULL || out == NULL) return -1;
    if (!MemoDone(m)) return 0;
    return TryTexture(m, out);
}

bool FreeplayIcon_Busy(void) { return busy; }

/* Textura da musica atual, se ja subiu. Renova o LRU para ela nao ser despejada enquanto aparece. */
Texture2D FreeplayIcon_Texture(void) {
    if (curSong == NULL) return (Texture2D){0};
    const IconMemo* m = MemoRequest(curSong);
    if (m == NULL || !MemoDone(m) || m->path[0] == '\0') return (Texture2D){0};
    IconTexture* t = TextureSlot(m->path);
    if (t == NULL) return (Texture2D){0};
    t->stamp = ++useClock;
    return t->texture;
}

char FreeplayIcon_IsLoaded(void) { return FreeplayIcon_Texture().id != 0; }

const char* FreeplayIcon_Opponent(void) {
    const IconMemo* m = curSong ? MemoRequest(curSong) : NULL;
    if (m == NULL || !MemoDone(m)) return "...";
    return m->opponent[0] ? m->opponent : "unknown";
}
