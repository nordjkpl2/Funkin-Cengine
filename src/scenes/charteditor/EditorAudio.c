/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: audio do editor (Inst, Voices e vozes separadas andam juntos). */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorAudio.h"
#include "EditorTimeline.h"

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

void Audio_SetVolume(float v) {
    Music* s[4];
    int n = Audio_Streams(s);
    for(int i = 0; i < n; i++)
        SetMusicVolume(*s[i], v);
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
