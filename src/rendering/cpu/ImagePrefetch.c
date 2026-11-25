/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Pre-decodificacao de PNG: PngDecode (Wuffs) numa thread de fundo, envio para a GPU na thread principal.
   Imagens prontas e nao usadas ficam num cache LRU pequeno (PF_CACHE_CAP_BYTES) para reuso. */
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "rendering/cpu/ImagePrefetch.h"
#include "core/Log.h"
#include "rendering/cpu/PngDecode.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"

#define PF_MAX 40
#define PF_PATH_MAX 260

#define PF_CACHE_MAX 64
#define PF_CACHE_CAP_BYTES ((size_t)15 * 1024 * 1024)

enum { PF_FREE = 0, PF_RUNNING = 1, PF_READY = 2, PF_FAILED = 3 };

typedef struct {
    char path[PF_PATH_MAX];
    Image img;
    atomic_int state;
    size_t bytes; // memoria estimada da imagem decodificada (RGBA)
} PfSlot;

typedef struct {
    char path[PF_PATH_MAX]; // vazio = entrada livre
    Image img;              // dono: o cache (UnloadImage ao despejar)
    size_t bytes;
    unsigned long stamp;    // ultimo uso (LRU)
} PfCacheEntry;

static PfSlot slots[PF_MAX];
static size_t usedBytes = 0; // so a thread principal mexe
static PfCacheEntry cache[PF_CACHE_MAX];
static size_t cacheBytes = 0;
static unsigned long cacheTick = 0;
static ImagePrefetchStats stats = {0}; // so a thread principal mexe

const ImagePrefetchStats* ImagePrefetch_Stats(void) {
    return &stats;
}

/* Caminhos recusados (memoria curta): pedidos repetidos nao abrem o arquivo nem geram log de novo.
   A lista so e esvaziada no ImagePrefetch_Clear (troca de cena), quando a recusa pode deixar de valer. */
#define PF_REFUSED_MAX 64
static char refusedPaths[PF_REFUSED_MAX][PF_PATH_MAX];
static int refusedCount = 0;

static bool Pf_IsRefused(const char* path) {
    for(int i = 0; i < refusedCount; i++)
        if(strcmp(refusedPaths[i], path) == 0)
            return true;
    return false;
}

static void Pf_ForgetRefused(void) {
    refusedCount = 0;
}

/* Conta sempre; registra no log so na primeira vez que o caminho e recusado. */
static void Pf_Refuse(unsigned* counter, const char* reason, const char* path) {
    (*counter)++;
    if(Pf_IsRefused(path))
        return;
    Log_WarnF("ImagePrefetch: %s (%s)", reason, path);
    if(refusedCount < PF_REFUSED_MAX) {
        memcpy(refusedPaths[refusedCount], path, strlen(path) + 1);
        refusedCount++;
    }
}

// Largura e altura do PNG lendo so o cabecalho (24 bytes). 0 se nao for PNG.
static int Pf_PngSize(const char* path, unsigned* w, unsigned* h) {
    unsigned char b[24];
    FILE* f = fopen(path, "rb");
    if(f == NULL)
        return 0;
    size_t n = fread(b, 1, sizeof(b), f);
    fclose(f);
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if(n < sizeof(b) || memcmp(b, sig, sizeof(sig)) != 0)
        return 0;
    *w = ((unsigned)b[16] << 24) | ((unsigned)b[17] << 16) | ((unsigned)b[18] << 8) | b[19];
    *h = ((unsigned)b[20] << 24) | ((unsigned)b[21] << 16) | ((unsigned)b[22] << 8) | b[23];
    return *w > 0 && *h > 0;
}

static PfSlot* Pf_Find(const char* path) {
    for(int i = 0; i < PF_MAX; i++)
        if(atomic_load(&slots[i].state) != PF_FREE && strcmp(slots[i].path, path) == 0)
            return &slots[i];
    return NULL;
}

/* Libera o slot sem mexer na imagem (quem chama ja a pegou ou a descarregou). */
static void Pf_Release(PfSlot* s) {
    usedBytes -= (usedBytes >= s->bytes) ? s->bytes : usedBytes;
    s->bytes = 0;
    s->path[0] = '\0';
    memset(&s->img, 0, sizeof(s->img));
    atomic_store(&s->state, PF_FREE);
}

/* ---------- cache LRU (so a thread principal mexe) ---------- */

static PfCacheEntry* Cache_Find(const char* path) {
    for(int i = 0; i < PF_CACHE_MAX; i++)
        if(cache[i].path[0] != '\0' && strcmp(cache[i].path, path) == 0)
            return &cache[i];
    return NULL;
}

static PfCacheEntry* Cache_Lru(void) {
    PfCacheEntry* oldest = NULL;
    for(int i = 0; i < PF_CACHE_MAX; i++)
        if(cache[i].path[0] != '\0' && (oldest == NULL || cache[i].stamp < oldest->stamp))
            oldest = &cache[i];
    return oldest;
}

static void Cache_Evict(PfCacheEntry* e) {
    cacheBytes -= (cacheBytes >= e->bytes) ? e->bytes : cacheBytes;
    UnloadImage(e->img);
    memset(e, 0, sizeof(*e));
}

/* Guarda 'img' no cache (assume a posse). Despeja os mais antigos se precisar.
   Devolve a entrada, ou NULL se a imagem for maior que o cache inteiro (ela e descarregada). */
static PfCacheEntry* Cache_Put(const char* path, Image img) {
    const size_t bytes = (size_t)img.width * img.height * 4;
    if(bytes > PF_CACHE_CAP_BYTES) {
        UnloadImage(img);
        return NULL;
    }

    PfCacheEntry* old = Cache_Find(path);
    if(old != NULL)
        Cache_Evict(old);

    PfCacheEntry* dst = NULL;
    for(;;) {
        for(int i = 0; i < PF_CACHE_MAX && dst == NULL; i++)
            if(cache[i].path[0] == '\0')
                dst = &cache[i];
        if(dst != NULL && cacheBytes + bytes <= PF_CACHE_CAP_BYTES)
            break;
        PfCacheEntry* victim = Cache_Lru();
        if(victim == NULL)
            break; // cache vazio e ainda sem espaco: nao deveria acontecer
        Cache_Evict(victim);
        dst = NULL;
    }
    if(dst == NULL) {
        UnloadImage(img);
        return NULL;
    }

    memcpy(dst->path, path, strlen(path) + 1);
    dst->img = img;
    dst->bytes = bytes;
    dst->stamp = ++cacheTick;
    cacheBytes += bytes;
    return dst;
}

/* ---------- decodificacao ---------- */

// thread de fundo: so CPU (PngDecode = ler arquivo + decodificar)
static void Pf_Job(void* data) {
    PfSlot* s = (PfSlot*)data;
    s->img = PngDecode_Load(s->path);
    atomic_store_explicit(&s->state, s->img.data != NULL ? PF_READY : PF_FAILED, memory_order_release);
}

/* Decodificar costuma levar poucos ms: cede a CPU algumas vezes (Sleep(0)) antes de dormir de
   verdade. Sleep(1) no Windows pode custar ate ~15 ms com a resolucao padrao do timer. */
#define PF_SPIN_YIELDS 256

static void Pf_Wait(PfSlot* s) {
    for(unsigned spins = 0; atomic_load_explicit(&s->state, memory_order_acquire) == PF_RUNNING; spins++) {
        Jobs_Poll();
        Thread_SleepMs(spins < PF_SPIN_YIELDS ? 0 : 1);
    }
}

/* Tira o slot de uso: imagem pronta vai para o cache (reuso), falha so libera. Espera o job antes. */
static void Pf_Detach(PfSlot* s) {
    Pf_Wait(s);
    if(atomic_load_explicit(&s->state, memory_order_acquire) == PF_READY)
        Cache_Put(s->path, s->img);
    Pf_Release(s);
}

bool ImagePrefetch_Start(const char* path) {
    if(path == NULL || path[0] == '\0' || strlen(path) >= PF_PATH_MAX) {
        stats.pathInvalid++; // sem caminho guardavel: so conta
        return false;
    }
    if(Pf_Find(path) != NULL)
        return true; // ja pedido (em andamento ou pronto): nao e falha
    PfCacheEntry* cached = Cache_Find(path);
    if(cached != NULL) {
        cached->stamp = ++cacheTick; // ja decodificado: nao decodifica de novo
        return true;
    }

    // Ordem: tudo que nao exige I/O primeiro. Recusa repetida sai aqui, sem tocar no disco.
    if(Pf_IsRefused(path)) {
        stats.refusedAgain++;
        return false;
    }

    // Slots prontos e nao usados viram cache, liberando espaco. So os RUNNING continuam ocupados.
    PfSlot* s = NULL;
    for(int i = 0; i < PF_MAX && s == NULL; i++)
        if(atomic_load(&slots[i].state) == PF_FREE)
            s = &slots[i];
    if(s == NULL) {
        for(int i = 0; i < PF_MAX; i++) {
            const int st = atomic_load(&slots[i].state);
            if(st == PF_READY || st == PF_FAILED)
                Pf_Detach(&slots[i]);
        }
        for(int i = 0; i < PF_MAX && s == NULL; i++)
            if(atomic_load(&slots[i].state) == PF_FREE)
                s = &slots[i];
    }
    if(s == NULL) {
        Pf_Refuse(&stats.noSlot, "todos os slots ocupados, carrega na hora", path);
        return false;
    }

    // A partir daqui ha I/O (ler o cabecalho do PNG); so depois de ter slot livre.
    unsigned w, h;
    if(!Pf_PngSize(path, &w, &h)) {
        Pf_Refuse(&stats.notPng, "nao e PNG valido", path);
        return false;
    }
    if(w > 16384 || h > 16384) {
        Pf_Refuse(&stats.tooBig, "imagem maior que 16384 px", path);
        return false;
    }
    size_t bytes = (size_t)w * h * 4;
    if(usedBytes + bytes > (size_t)IMAGE_PREFETCH_CAP_MB * 1024 * 1024) {
        Pf_Refuse(&stats.overBudget, "sem orcamento de memoria, carrega na hora", path);
        return false;
    }

    memcpy(s->path, path, strlen(path) + 1);
    memset(&s->img, 0, sizeof(s->img));
    s->bytes = bytes;
    atomic_store(&s->state, PF_RUNNING);
    usedBytes += bytes;

    Jobs_Init(0); // so cria na primeira vez
    if(!Jobs_Submit(Pf_Job, s, NULL)) {
        Pf_Refuse(&stats.submitFailed, "fila de jobs recusou, carrega na hora", path);
        Pf_Release(s);
        return false;
    }
    return true;
}

/* Ja da para subir sem esperar: decodificado com sucesso, falhou (Upload decide) ou esta no cache. */
bool ImagePrefetch_Ready(const char* path) {
    if(path == NULL || path[0] == '\0')
        return false;
    const PfSlot* s = Pf_Find(path);
    if(s != NULL)
        return atomic_load(&s->state) != PF_RUNNING;
    return Cache_Find(path) != NULL;
}

/* Envia a imagem pronta para a GPU direto da memoria do cache (sem copia). A imagem continua
   no cache para revisitas; so a imagem que nao cabe no cache e descarregada aqui. */
bool ImagePrefetch_Upload(const char* path, Texture2D* out) {
    if(path == NULL || out == NULL)
        return false;

    PfSlot* s = Pf_Find(path);
    if(s != NULL) {
        Pf_Wait(s);
        if(atomic_load_explicit(&s->state, memory_order_acquire) != PF_READY) {
            Pf_Refuse(&stats.decodeFailed, "decodificacao em fundo falhou, tenta de novo na hora", path);
            Pf_Release(s);
            return false;
        }
        const size_t bytes = (size_t)s->img.width * s->img.height * 4;
        if(bytes > PF_CACHE_CAP_BYTES) {
            // Grande demais para o cache: sobe e descarrega (nada de decodificar de novo).
            Image img = s->img;
            Pf_Release(s);
            *out = LoadTextureFromImage(img);
            UnloadImage(img);
            return true;
        }
        // Imagem fica no cache (para revisitas): a posse foi para o cache.
        PfCacheEntry* e = Cache_Put(path, s->img);
        Pf_Release(s);
        if(e != NULL) {
            e->stamp = ++cacheTick;
            *out = LoadTextureFromImage(e->img);
            return true;
        }
        Pf_Refuse(&stats.decodeFailed, "sem memoria no cache, tenta de novo na hora", path);
        return false;
    }

    PfCacheEntry* e = Cache_Find(path);
    if(e != NULL) {
        e->stamp = ++cacheTick;
        *out = LoadTextureFromImage(e->img);
        stats.cacheHit++;
        return true;
    }

    stats.takeMiss++;
    return false;
}

void ImagePrefetch_Clear(void) {
    for(int i = 0; i < PF_MAX; i++) {
        PfSlot* s = &slots[i];
        if(atomic_load(&s->state) == PF_FREE)
            continue;
        Pf_Detach(s); // pronto -> cache, falha -> libera
    }
    Pf_ForgetRefused(); // troca de cena: uma recusa antiga pode nao valer mais
    Jobs_Poll(); // libera as entradas ja concluidas da fila
}
