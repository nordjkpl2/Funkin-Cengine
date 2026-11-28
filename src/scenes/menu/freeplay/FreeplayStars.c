/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * FreeplayStars.c - dificuldade da musica no Freeplay, em estrelas.
 *
 * A nota vem do numero de notas do jogador no chart padrao (escala log: dobrar as notas = +0.9 estrela;
 * 50 notas = 0.5, ~1000 = 4.5, ~65000 = 10).
 * Musica longa conta como mais dificil, de proposito. A contagem le o chart inteiro, entao roda em fundo
 * (Jobs), uma vez por musica.
 */
#include "scenes/menu/freeplay/FreeplayStars.h"
#include "data/song/Song.h"
#include "threads/cpu/Jobs.h"
#include "threads/Thread.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>

#define STARS_BASE_NOTES 50.0f    /* 50 notas = meia estrela */
#define STARS_PER_DOUBLE 0.9f     /* estrelas a mais cada vez que as notas dobram */
#define STAR_INNER 0.45f          /* raio interno / externo da estrela */
#define STAR_GAP 1.15f            /* passo entre estrelas, em tamanhos */

enum { STARS_IDLE = 0, STARS_BUSY = 1, STARS_DONE = 2 };

typedef struct {
    const SongEntry* song;
    int notes;                 /* escrito pela thread de fundo antes de STARS_DONE */
    atomic_int state;
} StarsMemo;

static const SongEntry* memoBase;
static StarsMemo* memo;
static int memoCount;
static atomic_bool cancel;     /* Unload: contagens ainda na fila terminam sem ler o chart */

static void CountJob(void* data) {
    StarsMemo* m = (StarsMemo*)data;
    m->notes = atomic_load_explicit(&cancel, memory_order_relaxed) ? -1 : Song_CountPlayerNotes(m->song->dir);
    atomic_store_explicit(&m->state, STARS_DONE, memory_order_release);
}

static StarsMemo* Memo(const SongEntry* song) {
    if (!memo || !song || song < memoBase || song >= memoBase + memoCount) return NULL;
    return memo + (song - memoBase);
}

void FreeplayStars_Init(const SongEntry* songs, int count) {
    FreeplayStars_Unload();
    memoBase = songs;
    memoCount = count > 0 ? count : 0;
    memo = memoCount > 0 ? calloc((size_t)memoCount, sizeof(StarsMemo)) : NULL;
    if (memo == NULL) {
        memoCount = 0;
        return;
    }
    for (int i = 0; i < memoCount; ++i) memo[i].song = songs + i;
}

void FreeplayStars_Unload(void) {
    atomic_store(&cancel, true);
    for (int i = 0; i < memoCount; ++i)
        while (atomic_load_explicit(&memo[i].state, memory_order_acquire) == STARS_BUSY)
            Thread_SleepMs(0);
    atomic_store(&cancel, false);

    free(memo);
    memo = NULL;
    memoCount = 0;
    memoBase = NULL;
}

void FreeplayStars_Request(const SongEntry* song) {
    StarsMemo* m = Memo(song);
    if (m == NULL || atomic_load_explicit(&m->state, memory_order_acquire) != STARS_IDLE) return;
    atomic_store_explicit(&m->state, STARS_BUSY, memory_order_relaxed);
    Jobs_Init(0);
    if (!Jobs_Submit(CountJob, m, NULL)) CountJob(m); /* fila recusou: conta agora (raro) */
}

int FreeplayStars_Notes(const SongEntry* song) {
    const StarsMemo* m = Memo(song);
    if (m == NULL) return -1;
    if (atomic_load_explicit(&m->state, memory_order_acquire) != STARS_DONE) return -2;
    return m->notes;
}

float FreeplayStars_Rating(int notes) {
    if (notes <= 0) return 0.0f;
    float stars = 0.5f + STARS_PER_DOUBLE * log2f((float)notes / STARS_BASE_NOTES);
    stars = fminf((float)FREEPLAY_STARS_MAX, fmaxf(0.5f, stars));
    return roundf(stars * 2.0f) * 0.5f;
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

static Color Lerp(Color a, Color b, float t) {
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

/* Facil = ciano, medio = rosa, maximo = vermelho. */
static Color RatingColor(float rating) {
    static const Color easy = {120, 230, 255, 255};
    static const Color mid = {255, 92, 176, 255};
    static const Color hard = {255, 56, 64, 255};
    const float t = fminf(1.0f, fmaxf(0.0f, rating / (float)FREEPLAY_STARS_MAX));
    return t < 0.5f ? Lerp(easy, mid, t * 2.0f) : Lerp(mid, hard, (t - 0.5f) * 2.0f);
}

/* Estrela de 5 pontas em leque a partir do centro. Pontos em sentido anti-horario na tela (o raylib
 * descarta o outro sentido). half = so a metade esquerda (do topo ate a ponta interna de baixo). */
static void DrawStar(Vector2 c, float radius, bool half, Color color) {
    Vector2 points[12];
    const int edges = half ? 5 : 10;
    points[0] = c;
    for (int k = 0; k <= edges; ++k) {
        const float r = (k & 1) ? radius * STAR_INNER : radius;
        const float a = (-90.0f - 36.0f * (float)k) * DEG2RAD;
        points[k + 1] = (Vector2){c.x + cosf(a) * r, c.y + sinf(a) * r};
    }
    DrawTriangleFan(points, edges + 2, color);
}

void FreeplayStars_Draw(Vector2 pos, float size, float stars, float rating, float alpha) {
    const float radius = size * 0.5f;
    const Color empty = Fade((Color){50, 44, 80, 255}, alpha);
    const Color shadow = Fade((Color){0, 0, 0, 120}, alpha);
    const Color fill = Fade(RatingColor(rating), alpha);

    for (int i = 0; i < FREEPLAY_STARS_MAX; ++i) {
        /* A ponta de baixo fica mais alta que a de cima: desce um pouco para centralizar na caixa. */
        const Vector2 c = {pos.x + radius + (float)i * size * STAR_GAP, pos.y + radius * 1.1f};
        const float amount = stars - (float)i;
        DrawStar((Vector2){c.x + 2.0f, c.y + 2.0f}, radius, false, shadow);
        DrawStar(c, radius, false, empty);
        if (amount >= 1.0f) DrawStar(c, radius, false, fill);
        else if (amount >= 0.5f) DrawStar(c, radius, true, fill);
    }
}
