/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * AudioStretch.c - velocidade da musica sem mudar o tom (WSOLA, estilo SoundTouch).
 *
 * Caminho: decoder -> conversor do raylib (pitch = speed: so faz o decoder andar mais rapido)
 * -> processor daqui -> mixer. Por faixa, N frames entram e N saem:
 *   1) sinc janelado (Kaiser, 32 taps) reamostra por speed: volta o tom, o audio fica speed x mais longo;
 *   2) WSOLA: trechos de 40-90 ms emendados (crossfade de 8 ms) no ponto de maior correlacao,
 *      voltando ao tamanho original. Feito para musica (a Sonic era para voz e picotava).
 * O processor roda na thread de audio com o lock do raylib; o estado so e iniciado antes do
 * Attach e liberado depois do Detach (que tambem pega o lock), entao nao ha corrida.
 */

#include "scenes/gameplay/audio/AudioStretch.h"
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STRETCH_RATE 48000  /* = AUDIO_DEVICE_SAMPLE_RATE (lib/raylib55/src/config.h) */
#define CH 2                /* formato do mixer do raylib: float stereo */
#define MAX_CALL 512        /* frames por chamada do processor (tempBuffer do mixer) */

#define TAPS 32
#define HALF (TAPS / 2)
#define PHASES 128          /* fases da tabela do sinc (interpoladas entre si) */
#define KAISER_BETA 6.0     /* ~63 dB de rejeicao */

#define OVL (STRETCH_RATE * 8 / 1000)        /* crossfade entre trechos */
#define SEEK_CAP (STRETCH_RATE * 20 / 1000)  /* busca maxima (em 0.5x) */
#define RIN_CAP (MAX_CALL + TAPS + 8)
#define WIN_CAP 8192        /* > seq + seek + entrada de uma chamada, em 0.5x..2x */
#define OUT_CAP 16384       /* > 2 trechos + folga */
#define BLOCKS 32

#define STRETCH_PI 3.14159265358979323846

/* Trecho na fila de saida. pos em frames de musica (STRETCH_RATE) desde o Init. */
typedef struct {
    int64_t pos;
    int len;
} OutBlock;

typedef struct {
    float rin[RIN_CAP * CH];      /* saida do raylib (tom alto) + historico do sinc */
    float win[WIN_CAP * CH];      /* ja no tom original; win[0] = frame de musica winBase */
    float out[OUT_CAP * CH];      /* pronto para sair: out[outRead..outCount) */
    float mid[OVL * CH];          /* continuacao do ultimo trecho: entra no crossfade do proximo */
    float ref[OVL];               /* mid em mono com janela, para a correlacao */
    float mono[SEEK_CAP + OVL];
    float fade[OVL];
    float coef[(PHASES + 1) * TAPS];
    OutBlock blocks[BLOCKS];
    double speed, step, rpos;     /* rpos: leitura do sinc em rin (frames) */
    double skip, skipAcc;         /* avanco por trecho na entrada: speed * (seq - OVL) */
    double delayAvg;              /* atraso medio: dentro do trecho a musica anda 1x e pula entre trechos */
    float refE, refE2;            /* energia de ref (todas as amostras / so as pares) */
    int64_t recv, winBase, lastPos;
    int seq, seek;
    int rinCount, winCount, outRead, outCount, blockHead, blockCount;
    char first, primed;
} Stretch;

typedef struct {
    Stretch* st;
    AudioStream stream;           /* stream com o processor ligado */
    float pitch;                  /* pitch aplicado no conversor (raylib 5.5 aplica relativo, ver SetPitch) */
    atomic_int delay;             /* frames de musica entre o que o raylib entregou e o que esta saindo */
    char attached;
} StretchSlot;

static StretchSlot slots[AUDIOSTRETCH_SLOTS] = {{.pitch = 1.0f}, {.pitch = 1.0f}, {.pitch = 1.0f}, {.pitch = 1.0f}};

static double BesselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40 && term > sum * 1e-12; k++) {
        const double h = x / (2.0 * k);
        term *= h * h;
        sum += term;
    }
    return sum;
}

/* cutoff em ciclos por frame de entrada. Cada fase com ganho DC 1. */
static void BuildKernel(Stretch* st, double cutoff) {
    const double i0b = BesselI0(KAISER_BETA);
    for (int p = 0; p <= PHASES; p++) {
        float* c = st->coef + (size_t)p * TAPS;
        const double frac = (double)p / PHASES;
        double sum = 0.0;
        for (int k = 0; k < TAPS; k++) {
            const double d = (double)(k - HALF + 1) - frac; // distancia ate o ponto lido
            const double x = 2.0 * cutoff * d;
            const double sinc = fabs(x) < 1e-9 ? 1.0 : sin(STRETCH_PI * x) / (STRETCH_PI * x);
            const double r = d / HALF;
            const double w = fabs(r) >= 1.0 ? 0.0 : BesselI0(KAISER_BETA * sqrt(1.0 - r * r)) / i0b;
            c[k] = (float)(sinc * w);
            sum += c[k];
        }
        for (int k = 0; k < TAPS; k++) c[k] = (float)(c[k] / sum);
    }
}

static void Init(Stretch* st, double speed) {
    /* Parametros automaticos do SoundTouch: 90/20 ms em 0.5x ate 40/15 ms em 2x. */
    const double seqMs = fmin(fmax(106.67 - 33.33 * speed, 40.0), 90.0);
    const double seekMs = fmin(fmax(21.67 - 3.33 * speed, 15.0), 20.0);
    st->seq = (int)lround(STRETCH_RATE * seqMs / 1000.0);
    st->seek = (int)lround(STRETCH_RATE * seekMs / 1000.0);
    if (st->seek > SEEK_CAP) st->seek = SEEK_CAP;

    st->speed = speed;
    st->step = 1.0 / speed;
    st->skip = speed * (st->seq - OVL);
    st->skipAcc = 0.0;
    BuildKernel(st, 0.47 * fmin(1.0, speed)); // abaixo do Nyquist da saida quando a taxa desce

    for (int k = 0; k < OVL; k++) st->fade[k] = (float)(0.5 - 0.5 * cos(STRETCH_PI * (k + 0.5) / OVL));

    memset(st->rin, 0, sizeof(float) * HALF * CH); // historico vazio: o 1o frame real fica em rin[HALF]
    st->rinCount = HALF;
    st->rpos = HALF;
    st->winCount = st->outRead = st->outCount = st->blockHead = st->blockCount = 0;
    st->recv = st->winBase = 0;
    st->lastPos = -1;
    st->first = 1;
    st->primed = 0;
}

/* rin -> win: frame de musica j = frame de entrada j / speed. */
static void Resample(Stretch* st) {
    while (st->winCount < WIN_CAP) {
        const int i0 = (int)st->rpos;
        if (i0 + HALF >= st->rinCount) break;
        const double pf = (st->rpos - i0) * PHASES;
        const int p = (int)pf;
        const float a = (float)(pf - p);
        const float* c0 = st->coef + (size_t)p * TAPS;
        const float* x = st->rin + (size_t)(i0 - HALF + 1) * CH;
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < TAPS; k++) {
            const float c = c0[k] + a * (c0[k + TAPS] - c0[k]);
            l += c * x[2 * k];
            r += c * x[2 * k + 1];
        }
        float* w = st->win + (size_t)st->winCount++ * CH;
        w[0] = l;
        w[1] = r;
        st->rpos += st->step;
    }

    const int drop = (int)st->rpos - HALF + 1; // frames que o sinc nao usa mais
    if (drop > 0) {
        memmove(st->rin, st->rin + (size_t)drop * CH, sizeof(float) * (size_t)(st->rinCount - drop) * CH);
        st->rinCount -= drop;
        st->rpos -= drop;
    }
}

static float Score(const Stretch* st, const float* m, int o, int stride) {
    float cr = 0.0f, nr = 0.0f;
    for (int k = 0; k < OVL; k += stride) {
        cr += st->ref[k] * m[k];
        nr += m[k] * m[k];
    }
    const float corr = cr / sqrtf((stride == 1 ? st->refE : st->refE2) * nr + 1e-12f); // -1..1
    const float t = (float)(2 * o - st->seek) / (float)st->seek;
    return (corr + 1.0f) * (1.0f - 0.15f * t * t); // leve preferencia pelo meio: menos deriva entre faixas
}

/* Onde em win[0..seek) o trecho novo encaixa melhor na continuacao (mid). Busca grossa, depois fina. */
static int BestOffset(Stretch* st) {
    float* m = st->mono;
    for (int i = 0; i < st->seek + OVL; i++) m[i] = st->win[2 * i] + st->win[2 * i + 1];

    int best = 0;
    float bestScore = -1e30f;
    for (int o = 0; o < st->seek; o += 2) {
        const float sc = Score(st, m + o, o, 2);
        if (sc > bestScore) bestScore = sc, best = o;
    }

    const int lo = best > 2 ? best - 2 : 0;
    const int hi = best + 2 < st->seek ? best + 2 : st->seek - 1;
    bestScore = -1e30f;
    for (int o = lo; o <= hi; o++) {
        const float sc = Score(st, m + o, o, 1);
        if (sc > bestScore) bestScore = sc, best = o;
    }
    return best;
}

/* Um trecho: (seq - OVL) frames para out, avanca speed * (seq - OVL) em win. */
static void Iterate(Stretch* st) {
    const int len = st->seq - OVL;
    const int off = st->first ? 0 : BestOffset(st);
    const float* w = st->win + (size_t)off * CH;

    if (st->outCount + len > OUT_CAP) { // compacta a fila (o chamador garante que cabe)
        memmove(st->out, st->out + (size_t)st->outRead * CH, sizeof(float) * (size_t)(st->outCount - st->outRead) * CH);
        st->outCount -= st->outRead;
        st->outRead = 0;
    }
    float* o = st->out + (size_t)st->outCount * CH;
    if (st->first) {
        memcpy(o, w, sizeof(float) * (size_t)len * CH);
    } else {
        for (int k = 0; k < OVL * CH; k++) {
            const float f = st->fade[k / CH];
            o[k] = st->mid[k] + f * (w[k] - st->mid[k]);
        }
        memcpy(o + OVL * CH, w + OVL * CH, sizeof(float) * (size_t)(len - OVL) * CH);
    }
    st->outCount += len;
    st->first = 0;

    OutBlock* b = &st->blocks[(st->blockHead + st->blockCount++) % BLOCKS];
    b->pos = st->winBase + off;
    b->len = len;

    memcpy(st->mid, w + (size_t)len * CH, sizeof(st->mid));
    float e = 0.0f, e2 = 0.0f;
    for (int k = 0; k < OVL; k++) {
        const float f = st->fade[k];
        const float v = (st->mid[2 * k] + st->mid[2 * k + 1]) * 4.0f * f * (1.0f - f);
        st->ref[k] = v;
        e += v * v;
        if ((k & 1) == 0) e2 += v * v;
    }
    st->refE = e;
    st->refE2 = e2;

    st->skipAcc += st->skip;
    const int n = (int)st->skipAcc;
    st->skipAcc -= n;
    memmove(st->win, st->win + (size_t)n * CH, sizeof(float) * (size_t)(st->winCount - n) * CH);
    st->winCount -= n;
    st->winBase += n;
}

static void Output(Stretch* st, float* buf, int frames) {
    // folga antes de soltar audio: os trechos chegam em blocos de (seq - OVL)
    if (!st->primed && st->outCount - st->outRead >= st->seq - OVL + MAX_CALL) st->primed = 1;

    const int ready = st->outCount - st->outRead;
    int n = st->primed ? (frames < ready ? frames : ready) : 0;
    memcpy(buf, st->out + (size_t)st->outRead * CH, sizeof(float) * (size_t)n * CH);
    memset(buf + (size_t)n * CH, 0, sizeof(float) * (size_t)(frames - n) * CH);
    st->outRead += n;
    if (st->outRead == st->outCount) st->outRead = st->outCount = 0;

    while (n > 0 && st->blockCount > 0) { // posicao de musica do ultimo frame que saiu
        OutBlock* b = &st->blocks[st->blockHead];
        const int take = n < b->len ? n : b->len;
        st->lastPos = b->pos + take - 1;
        b->pos += take;
        b->len -= take;
        n -= take;
        if (b->len == 0) {
            st->blockHead = (st->blockHead + 1) % BLOCKS;
            st->blockCount--;
        }
    }
}

/* Thread de audio. */
static void Process(StretchSlot* slot, float* buf, unsigned int frames) {
    Stretch* st = slot->st;
    for (unsigned int done = 0; done < frames;) {
        int n = (int)(frames - done);
        if (n > MAX_CALL) n = MAX_CALL;
        float* chunk = buf + (size_t)done * CH;

        int fit = RIN_CAP - st->rinCount; // sempre cabe; so protege contra estado ruim
        if (fit > n) fit = n;
        memcpy(st->rin + (size_t)st->rinCount * CH, chunk, sizeof(float) * (size_t)fit * CH);
        st->rinCount += fit;
        st->recv += n;
        Resample(st);

        for (;;) {
            const int next = (int)(st->skipAcc + st->skip) + 1;
            const int need = st->seq + st->seek > next ? st->seq + st->seek : next;
            if (st->winCount < need || st->outCount - st->outRead + st->seq - OVL > OUT_CAP || st->blockCount == BLOCKS) break;
            Iterate(st);
        }

        Output(st, chunk, n);
        done += (unsigned int)n;
    }
    // O atraso real faz dente de serra (ate ~40 ms em 0.5x): usar direto tremeria as notas.
    const double delay = (double)st->recv * st->speed - (double)(st->lastPos + 1);
    if (st->lastPos < 0) st->delayAvg = delay; // ainda sem som: relogio parado no inicio
    else st->delayAvg += (delay - st->delayAvg) * fmin(1.0, frames / (0.5 * STRETCH_RATE));
    atomic_store_explicit(&slot->delay, (int)lround(st->delayAvg), memory_order_relaxed);
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
    atomic_store(&s->delay, 0);
}

void AudioStretch_Off(int slot, Music* music) {
    StretchSlot* s = Slot(slot);
    if (s == NULL) return;
    Detach(s, slot);
    free(s->st);
    s->st = NULL;
    if (music != NULL && music->stream.buffer != NULL) SetPitch(s, music, 1.0f);
    s->pitch = 1.0f; // stream novo (Load) sempre comeca em 1
}

void AudioStretch_Set(int slot, Music* music, float speed) {
    StretchSlot* s = Slot(slot);
    if (s == NULL || music == NULL || music->stream.buffer == NULL) return;

    Detach(s, slot);
    SetPitch(s, music, speed);
    if (fabsf(speed - 1.0f) < 0.001f) return;

    if (s->st == NULL) s->st = malloc(sizeof(Stretch)); // fica ate o Off: seek recria sem realocar
    if (s->st == NULL) return; // sem memoria: fica so o pitch (tom muda, mas toca)
    Init(s->st, speed);
    s->stream = music->stream;
    AttachAudioStreamProcessor(s->stream, procs[slot]);
    s->attached = 1;
}

double AudioStretch_Delay(int slot) {
    StretchSlot* s = Slot(slot);
    if (s == NULL || !s->attached) return 0.0;
    const int d = atomic_load_explicit(&s->delay, memory_order_relaxed);
    return d > 0 ? (double)d / STRETCH_RATE : 0.0;
}
