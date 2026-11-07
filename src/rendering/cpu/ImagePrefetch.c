/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Pre-decodificacao de PNG: PngDecode (Wuffs) numa thread de fundo, envio para a GPU na thread principal. */
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "ImagePrefetch.h"
#include "PngDecode.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"

#define PF_MAX 16
#define PF_PATH_MAX 260

enum { PF_FREE = 0, PF_RUNNING = 1, PF_READY = 2, PF_FAILED = 3 };

typedef struct {
    char path[PF_PATH_MAX];
    Image img;
    atomic_int state;
    size_t bytes; // memoria estimada da imagem decodificada (RGBA)
} PfSlot;

static PfSlot slots[PF_MAX];
static size_t usedBytes = 0; // so a thread principal mexe

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

static void Pf_Release(PfSlot* s) {
    usedBytes -= (usedBytes >= s->bytes) ? s->bytes : usedBytes;
    s->bytes = 0;
    s->path[0] = '\0';
    memset(&s->img, 0, sizeof(s->img));
    atomic_store(&s->state, PF_FREE);
}

// thread de fundo: so CPU (PngDecode = ler arquivo + decodificar)
static void Pf_Job(void* data) {
    PfSlot* s = (PfSlot*)data;
    s->img = PngDecode_Load(s->path);
    atomic_store_explicit(&s->state, s->img.data != NULL ? PF_READY : PF_FAILED, memory_order_release);
}

static void Pf_Wait(PfSlot* s) {
    while(atomic_load_explicit(&s->state, memory_order_acquire) == PF_RUNNING) {
        Jobs_Poll();
        Thread_SleepMs(1);
    }
}

void ImagePrefetch_Start(const char* path) {
    if(path == NULL || path[0] == '\0' || strlen(path) >= PF_PATH_MAX || Pf_Find(path) != NULL)
        return;

    unsigned w, h;
    if(!Pf_PngSize(path, &w, &h) || w > 16384 || h > 16384)
        return;
    size_t bytes = (size_t)w * h * 4;
    if(usedBytes + bytes > (size_t)IMAGE_PREFETCH_CAP_MB * 1024 * 1024)
        return; // sem memoria sobrando: carrega normal, na hora

    PfSlot* s = NULL;
    for(int i = 0; i < PF_MAX && s == NULL; i++)
        if(atomic_load(&slots[i].state) == PF_FREE)
            s = &slots[i];
    if(s == NULL)
        return;

    memcpy(s->path, path, strlen(path) + 1);
    memset(&s->img, 0, sizeof(s->img));
    s->bytes = bytes;
    atomic_store(&s->state, PF_RUNNING);
    usedBytes += bytes;

    Jobs_Init(0); // so cria na primeira vez
    if(!Jobs_Submit(Pf_Job, s, NULL))
        Pf_Release(s);
}

bool ImagePrefetch_Take(const char* path, Image* out) {
    if(path == NULL || out == NULL)
        return false;
    PfSlot* s = Pf_Find(path);
    if(s == NULL)
        return false;
    Pf_Wait(s);
    bool ok = atomic_load_explicit(&s->state, memory_order_acquire) == PF_READY;
    if(ok)
        *out = s->img;
    Pf_Release(s);
    return ok;
}

void ImagePrefetch_Clear(void) {
    for(int i = 0; i < PF_MAX; i++) {
        PfSlot* s = &slots[i];
        if(atomic_load(&s->state) == PF_FREE)
            continue;
        Pf_Wait(s);
        if(atomic_load_explicit(&s->state, memory_order_acquire) == PF_READY)
            UnloadImage(s->img);
        Pf_Release(s);
    }
    Jobs_Poll(); // libera as entradas ja concluidas da fila
}
