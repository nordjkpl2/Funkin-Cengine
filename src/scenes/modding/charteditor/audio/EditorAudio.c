/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: audio do editor (Inst, Voices e vozes separadas andam juntos). */

#include "scenes/modding/charteditor/audio/EditorAudio.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"

char playing = 0;

char playFollow = 1; // View: trava a grade no receptor durante o OUVIR, igual ao jogo

float edVolApplied = -1.0f;

Music edInst;

char edHasAudio = 0;

Music edVoices;

char edHasVoices = 0;

Music edSplitO;

Music edSplitP;

char edHasSplit = 0;

char edPlayHasInst = 0;
char edPlayHasVocals = 0;
char edPlayHasBf = 0;
char edPlayHasOpp = 0;

// ---------- audio (Inst, Voices e vozes separadas andam sempre juntos) ----------
// Ate 4 streams: o ponteiro e NULL quando a faixa nao existe.
static int Audio_Streams(Music* out[4]) {
    int n = 0;
    if(edHasAudio) out[n++] = &edInst;
    if(edHasVoices) out[n++] = &edVoices;
    if(edHasSplit) {
        out[n++] = &edSplitO;
        out[n++] = &edSplitP;
    }
    return n;
}

// Solo (menu Sound): as faixas nao escolhidas ficam em volume 0, mas seguem tocando (sync).
static float audioVol = 1.0f;
int edSoloMix = 0; // 0 tudo, 1 inst, 2 vozes, 3 bf, 4 opponent

static float StreamVolume(const Music* m) {
    const char splitVox = !edHasVoices && (edSoloMix == 2);
    if(edSoloMix == 0) return audioVol;
    if(m == &edInst) return edSoloMix == 1 ? audioVol : 0.0f;
    if(m == &edVoices) return edSoloMix == 2 ? audioVol : 0.0f;
    if(m == &edSplitP) return (splitVox || edSoloMix == 3) ? audioVol : 0.0f;
    if(m == &edSplitO) return (splitVox || edSoloMix == 4) ? audioVol : 0.0f;
    return 0.0f;
}

void Audio_ApplyVolumes(void) {
    Music* s[4];
    int n = Audio_Streams(s);
    for(int i = 0; i < n; i++)
        SetMusicVolume(*s[i], StreamVolume(s[i]));
}

void Audio_SetVolume(float v) {
    audioVol = v;
    Audio_ApplyVolumes();
}

void Audio_SetSolo(int mix) {
    edSoloMix = mix;
    Audio_ApplyVolumes();
}

static void Audio_Pause(void) {
    Music* s[4];
    int n = Audio_Streams(s);
    for(int i = 0; i < n; i++)
        PauseMusicStream(*s[i]);
}

void Audio_Update(void) {
    Music* s[4];
    int n = Audio_Streams(s);
    for(int i = 0; i < n; i++)
        UpdateMusicStream(*s[i]);
}

void Audio_SeekAll(double ms) {
    Music* s[4];
    int n = Audio_Streams(s);
    float pos = (float)(ms / 1000.0);
    for(int i = 0; i < n; i++)
        SeekMusicStream(*s[i], pos);
}

static void Audio_PlayFrom(double ms) {
    Music* s[4];
    int n = Audio_Streams(s);
    float pos = (float)(ms / 1000.0);
    for(int i = 0; i < n; i++) {
        SeekMusicStream(*s[i], pos);
        if(!IsMusicStreamPlaying(*s[i]))
            PlayMusicStream(*s[i]);
    }
}

void Audio_StopPlay(void) {
    playing = 0;
    Audio_Pause();
}

void Audio_Toggle(void) {
    if(playing) {
        Audio_StopPlay();
        return;
    }
    if(cursorMs >= SongEndMs())
        cursorMs = 0;
    Audio_PlayFrom(cursorMs);
    playing = 1;
}
