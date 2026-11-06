/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: tempo do chart (inicio dos trechos, cursor, janela visivel). */
#include"scenes/AllScenes.h"
#include"scenes/Song.h"
#include"scenes/SongList.h"
#include "scenes/ModPath.h"
#include "scenes/Character.h"
#include"scenes/Stage.h"
#include"scenes/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorTimeline.h"
#include "EditorDataAndIO.h"

// editor
int secIdx = 0;

int snapDiv = 4; // 1, 2 ou 4 (por tempo)

double cursorMs = 0;

double viewTopMs = 0;

// tempo (ms) onde cada trecho comeca (pela duracao dos anteriores)
static double* secTimes = NULL; // prefixo: [i] = inicio do trecho i, [nsec] = fim

static int secTimesN = -1;      // nsec coberto; -1 = invalido

void SecTimes_Drop(void) {
    free(secTimes);
    secTimes = NULL;
    secTimesN = -1;
}

void SecTimes_Invalidate(void) {
    secTimesN = -1;
}

static void SecTimes_Ensure(void) {
    if(secTimesN == chart.nsec && secTimes != NULL)
        return;
    free(secTimes);
    secTimes = NULL;
    secTimesN = -1;
    if(chart.nsec < 0)
        return;
    secTimes = malloc(sizeof(double) * ((size_t)chart.nsec + 1));
    if(secTimes == NULL)
        return;
    double t = 0;
    for(int i = 0; i < chart.nsec; i++) {
        secTimes[i] = t;
        t += chart.secs[i].lenSteps * (60000.0 / chart.secs[i].bpm) / 4.0;
    }
    secTimes[chart.nsec] = t;
    secTimesN = chart.nsec;
}

double SecStartMs(int idx) {
    SecTimes_Ensure();
    if(secTimes == NULL || secTimesN < 0) {
        // sem cache (sem memoria): loop antigo
        double t = 0;
        for(int i = 0; i < idx && i < chart.nsec; i++)
            t += chart.secs[i].lenSteps * (60000.0 / chart.secs[i].bpm) / 4.0;
        return t;
    }
    if(idx < 0)
        return 0;
    if(idx > secTimesN)
        idx = secTimesN;
    return secTimes[idx];
}

double SongEndMs(void) {
    return SecStartMs(chart.nsec);
}

int SecAtTime(double ms) {
    SecTimes_Ensure();
    if(secTimes == NULL || secTimesN <= 0)
        return 0;
    // ultimo trecho com inicio <= ms (igual ao linear antigo, O(log n))
    if(ms < secTimes[0])
        return (chart.nsec > 0) ? chart.nsec - 1 : 0;
    int lo = 0, hi = secTimesN;
    while(lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if(secTimes[mid] <= ms)
            lo = mid;
        else
            hi = mid - 1;
    }
    if(lo >= chart.nsec)
        lo = chart.nsec - 1;
    return lo;
}

// Trechos que podem ter nota visivel entre dois tempos (com 1 trecho de folga
// de cada lado). Sem cache de tempos, devolve todos (comportamento antigo).
void SecRangeInView(double fromMs, double toMs, int* first, int* last) {
    SecTimes_Ensure();
    *first = 0;
    *last = chart.nsec - 1;
    if(secTimes == NULL || secTimesN <= 0 || chart.nsec <= 0)
        return;
    if(fromMs >= secTimes[0])
        *first = SecAtTime(fromMs) - 1;
    if(toMs >= secTimes[0])
        *last = SecAtTime(toMs) + 1;
    if(*first < 0) *first = 0;
    if(*last > chart.nsec - 1) *last = chart.nsec - 1;
}

// painel segue o cursor (a barra que move)
void SecFollowCursor(void) {
    if(chart.nsec > 0) {
        int si = SecAtTime(cursorMs < 0 ? 0 : cursorMs);
        if(si >= 0 && si < chart.nsec)
            secIdx = si;
    }
}
