/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * AudioStretch.c - velocidade da musica sem mudar o tom (Sonic, lib/sonic.c).
 *
 * Caminho: decoder -> conversor do raylib (pitch = speed: anda mais rapido e fica agudo)
 * -> processor daqui (Sonic com pitch 1/speed: volta o tom, mesma quantidade de frames) -> mixer.
 * O processor roda na thread de audio com o lock do raylib; a Sonic so e criada antes do
 * Attach e destruida depois do Detach (que tambem pega o lock), entao nao ha corrida.
 */

#include "scenes/gameplay/audio/AudioStretch.h"
#include "sonic.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

/* O raylib nao expoe a taxa do device; 48 kHz e o padrao do Windows. Errar (44.1k) so muda
   ~8% da faixa de busca de periodo da Sonic e do atraso estimado (~3 ms). */
#define STRETCH_RATE 48000
#define STRETCH_CHANNELS 2      /* formato do mixer do raylib: float stereo */
#define STRETCH_PRIME 1024      /* folga antes de soltar audio: evita buraco quando a Sonic entrega em blocos */

typedef struct {
    sonicStream sonic;
    AudioStream stream;         /* stream com o processor ligado */
    float pitch;                /* pitch aplicado no conversor (raylib 5.5 aplica relativo, ver SetPitch) */
    float speed;
    atomic_int lag;             /* frames dentro da Sonic (entrada - saida de conteudo) */
    char primed;
    char attached;
} StretchSlot;

static StretchSlot slots[AUDIOSTRETCH_SLOTS] = {
    {.pitch = 1.0f, .speed = 1.0f}, {.pitch = 1.0f, .speed = 1.0f},
    {.pitch = 1.0f, .speed = 1.0f}, {.pitch = 1.0f, .speed = 1.0f},
};

/* Thread de audio. frames <= 512 por chamada (buffer temporario do mixer). */
static void Process(StretchSlot* s, float* buf, unsigned int frames) {
    const unsigned int count = frames * STRETCH_CHANNELS;
    for (unsigned int i = 0; i < count; i++) // a Sonic converte para short sem limitar
        buf[i] = fminf(fmaxf(buf[i], -1.0f), 1.0f);

    sonicWriteFloatToStream(s->sonic, buf, (int)frames);

    int got = 0;
    if (!s->primed && sonicSamplesAvailable(s->sonic) >= STRETCH_PRIME) s->primed = 1;
    if (s->primed) got = sonicReadFloatFromStream(s->sonic, buf, (int)frames);
    if ((unsigned int)got < frames)
        memset(buf + (size_t)got * STRETCH_CHANNELS, 0, (size_t)(frames - (unsigned int)got) * STRETCH_CHANNELS * sizeof(float));

    atomic_fetch_add_explicit(&s->lag, (int)frames - got, memory_order_relaxed);
}

/* O processor do raylib nao tem ponteiro de usuario: uma funcao por slot. */
static void Proc0(void* b, unsigned int n) { Process(&slots[0], (float*)b, n); }
static void Proc1(void* b, unsigned int n) { Process(&slots[1], (float*)b, n); }
static void Proc2(void* b, unsigned int n) { Process(&slots[2], (float*)b, n); }
static void Proc3(void* b, unsigned int n) { Process(&slots[3], (float*)b, n); }
static AudioCallback const procs[AUDIOSTRETCH_SLOTS] = {Proc0, Proc1, Proc2, Proc3};

/* raylib 5.5: SetMusicPitch divide a taxa de saida JA alterada (acumula). Passa o fator relativo. */
static void SetPitch(StretchSlot* s, Music* music, float pitch) {
    if (fabsf(pitch - s->pitch) < 0.0001f) return;
    SetMusicPitch(*music, pitch / s->pitch);
    s->pitch = pitch;
}

static StretchSlot* Slot(int slot) {
    return (slot >= 0 && slot < AUDIOSTRETCH_SLOTS) ? &slots[slot] : NULL;
}

static void Detach(StretchSlot* s, int slot) {
    if (s->attached) DetachAudioStreamProcessor(s->stream, procs[slot]);
    s->attached = 0;
    if (s->sonic != NULL) sonicDestroyStream(s->sonic);
    s->sonic = NULL;
    atomic_store(&s->lag, 0);
}

void AudioStretch_Off(int slot, Music* music) {
    StretchSlot* s = Slot(slot);
    if (s == NULL) return;
    Detach(s, slot);
    if (music != NULL && music->stream.buffer != NULL) SetPitch(s, music, 1.0f);
    s->pitch = 1.0f; // stream novo (Load) sempre comeca em 1
    s->speed = 1.0f;
}

void AudioStretch_Set(int slot, Music* music, float speed) {
    StretchSlot* s = Slot(slot);
    if (s == NULL || music == NULL || music->stream.buffer == NULL) return;

    Detach(s, slot);
    SetPitch(s, music, speed);
    s->speed = speed;
    if (fabsf(speed - 1.0f) < 0.001f) return;

    s->sonic = sonicCreateStream(STRETCH_RATE, STRETCH_CHANNELS);
    if (s->sonic == NULL) return; // sem memoria: fica so o pitch (tom muda, mas toca)
    sonicSetPitch(s->sonic, 1.0f / speed);
    s->primed = 0;
    s->stream = music->stream;
    AttachAudioStreamProcessor(s->stream, procs[slot]);
    s->attached = 1;
}

double AudioStretch_Delay(int slot) {
    StretchSlot* s = Slot(slot);
    if (s == NULL || !s->attached) return 0.0;
    const int lag = atomic_load_explicit(&s->lag, memory_order_relaxed);
    return lag > 0 ? (double)lag / STRETCH_RATE * s->speed : 0.0;
}
