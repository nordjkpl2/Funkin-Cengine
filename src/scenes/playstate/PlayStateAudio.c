/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAudio.c - instrumental e vozes do PlayState.
 */

#include "scenes/playstate/PlayStateAudio.h"
#include <math.h>
#include <stdio.h>

static Music inst;
static Music voices;
static char hasVoices;
static char wasPaused;
static float lastVoiceSync = -10.0f;

// Mesmas extensoes aceitas pelo ModImport e pelo SongList.
static char FindAudioPath(const char* songDir, const char* baseName, char out[512]) {
    static const char* const extensions[] = {"ogg", "mp3", "wav"};

    if (out == NULL || baseName == NULL || baseName[0] == '\0') return 0;
    out[0] = '\0';

    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        int written = snprintf(out, 512, "%s/%s.%s", songDir, baseName, extensions[i]);
        if (written > 0 && written < 512 && FileExists(out)) return 1;
    }
    out[0] = '\0';
    return 0;
}

static Music LoadInstrumental(const char* songDir) {
    char path[512];
    Music music = {0};

    if (!FindAudioPath(songDir, "Inst", path)) {
        TraceLog(LOG_ERROR, "Instrumental not found (Inst.ogg/mp3/wav): %s", songDir);
        return music;
    }

    music = LoadMusicStream(path);
    if (music.stream.buffer == NULL) {
        TraceLog(LOG_ERROR, "Could not load instrumental: %s", path);
        return (Music){0};
    }

    music.looping = 0;
    SetMusicVolume(music, RayGame_MusicVolumeLevel());
    return music;
}

static void LoadVoices(const char* songDir) {
    char path[512];

    hasVoices = 0;
    voices = (Music){0};
    if (!FindAudioPath(songDir, "Voices", path)) return;

    voices = LoadMusicStream(path);
    if (voices.stream.buffer == NULL) {
        TraceLog(LOG_WARNING, "Could not load voices: %s", path);
        voices = (Music){0};
        return;
    }

    voices.looping = 0;
    SetMusicVolume(voices, RayGame_MusicVolumeLevel());
    hasVoices = 1;
}

void PlayStateAudio_Load(const char* songDir) {
    inst = LoadInstrumental(songDir);
    LoadVoices(songDir);
}

void PlayStateAudio_Unload(void) {
    UnloadMusicStream(inst);
    if (hasVoices) UnloadMusicStream(voices);
    hasVoices = 0;
}

void PlayStateAudio_Reset(void) {
    lastVoiceSync = -10.0f;
    wasPaused = 1;
}

void PlayStateAudio_StartPaused(void) {
    PlayMusicStream(inst);
    if (hasVoices) PlayMusicStream(voices);

    PauseMusicStream(inst);
    if (hasVoices) PauseMusicStream(voices);
    wasPaused = 1;
}

void PlayStateAudio_Pause(void) {
    PauseMusicStream(inst);
    if (hasVoices) PauseMusicStream(voices);
    wasPaused = 1;
}

void PlayStateAudio_UpdateStreams(void) {
    UpdateMusicStream(inst);
    if (hasVoices) UpdateMusicStream(voices);
}

void PlayStateAudio_Update(void) {
    PlayStateAudio_UpdateStreams();

    if (wasPaused) {
        ResumeMusicStream(inst);
        if (hasVoices) ResumeMusicStream(voices);
        wasPaused = 0;
    }
}

char PlayStateAudio_IsPlaying(void) {
    return IsMusicStreamPlaying(inst);
}

float PlayStateAudio_Length(void) {
    return GetMusicTimeLength(inst);
}

void PlayStateAudio_SyncVoices(void) {
    float time = GetMusicTimePlayed(inst);
    if (hasVoices && time - lastVoiceSync > 0.5f &&
        fabs(time - GetMusicTimePlayed(voices)) > 0.02f) {
        SeekMusicStream(voices, time);
        lastVoiceSync = time;
    }
}

void PlayStateAudio_SetVolume(float volume) {
    if (inst.stream.buffer != NULL)
        SetMusicVolume(inst, volume);

    if (hasVoices && voices.stream.buffer != NULL)
        SetMusicVolume(voices, volume);
}

Music* PlayStateAudio_Inst(void) {
    return &inst;
}
