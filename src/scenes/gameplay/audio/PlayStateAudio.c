/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAudio.c - instrumental e vozes do PlayState.
 *
 * Todas as faixas (Inst, Voices, Voices-Player, Voices-Opponent) tocam sempre juntas, para a
 * sincronia nao quebrar. O "mix" so decide quais tem volume: as silenciadas ficam em volume 0.
 */

#include "scenes/gameplay/audio/PlayStateAudio.h"
#include "scenes/gameplay/audio/AudioStretch.h"
#include <math.h>
#include <stdio.h>
#include "Game.h"
#include "core/Log.h"

// slots de faixa vocal
enum { VOX_MAIN = 0, VOX_PLAYER, VOX_OPP, VOX_COUNT };

static Music inst;
static char instOk;
static Music vox[VOX_COUNT];
static char voxOk[VOX_COUNT];
static char wasPaused;
static float lastVoiceSync = -10.0f;
static float appliedVolume = -1.0f;
static PlayStateAudioMix mix = PSA_MIX_FULL;
static float speed = 1.0f;   /* fica entre restarts (pratica); slot do stretch: 0 = inst, 1+i = vox[i] */

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

static char LoadStream(const char* songDir, const char* baseName, Music* out, const char* what) {
    char path[512];

    *out = (Music){0};
    if (!FindAudioPath(songDir, baseName, path)) return 0;

    *out = LoadMusicStream(path);
    if (out->stream.buffer == NULL) {
        Log_WarnF("Nao carregou %s: %s", what, path);
        *out = (Music){0};
        return 0;
    }
    out->looping = 0;
    return 1;
}

/* Volume efetivo de cada faixa conforme o mix. Slot 0 = inst; 1..3 = vox[slot-1]. */
static char Audible(int slot) {
    const char split = !voxOk[VOX_MAIN]; // sem Voices.ogg: vale o par separado
    switch (mix) {
        case PSA_MIX_INST:
            return slot == 0;
        case PSA_MIX_VOCALS:
            return slot == 1 + VOX_MAIN || (split && (slot == 1 + VOX_PLAYER || slot == 1 + VOX_OPP));
        case PSA_MIX_BF:
            return slot == 1 + VOX_PLAYER;
        case PSA_MIX_OPP:
            return slot == 1 + VOX_OPP;
        case PSA_MIX_FULL:
        default:
            if (slot == 0) return 1;
            if (slot == 1 + VOX_MAIN) return voxOk[VOX_MAIN];
            return split;
    }
}

static void ApplyVolumes(void) {
    const float volume = appliedVolume < 0 ? RayGame_MusicVolumeLevel() : appliedVolume;
    if (instOk) SetMusicVolume(inst, Audible(0) ? volume : 0.0f);
    for (int i = 0; i < VOX_COUNT; i++)
        if (voxOk[i]) SetMusicVolume(vox[i], Audible(1 + i) ? volume : 0.0f);
}

/* Stretch na velocidade atual em todas as faixas (recria os buffers: serve tambem depois de seek). */
static void ApplySpeed(void) {
    if (instOk) AudioStretch_Set(0, &inst, speed);
    for (int i = 0; i < VOX_COUNT; i++)
        if (voxOk[i]) AudioStretch_Set(1 + i, &vox[i], speed);
}

void PlayStateAudio_SetSpeed(float newSpeed) {
    if (!(newSpeed >= PSA_SPEED_MIN)) newSpeed = PSA_SPEED_MIN; // NaN cai aqui
    if (newSpeed > PSA_SPEED_MAX) newSpeed = PSA_SPEED_MAX;
    if (newSpeed == speed) return;
    speed = newSpeed;
    ApplySpeed();
}

float PlayStateAudio_Speed(void) {
    return speed;
}

double PlayStateAudio_OutputDelay(void) {
    return AudioStretch_Delay(0);
}

void PlayStateAudio_SetMix(PlayStateAudioMix newMix) {
    mix = newMix;
    ApplyVolumes();
}

void PlayStateAudio_Load(const char* songDir) {
    instOk = LoadStream(songDir, "Inst", &inst, "a Inst");
    if (!instOk) Log_WarnF("Inst nao encontrada (ogg/mp3/wav): %s", songDir);

    voxOk[VOX_MAIN] = LoadStream(songDir, "Voices", &vox[VOX_MAIN], "as Voices");
    voxOk[VOX_PLAYER] = LoadStream(songDir, "Voices-Player", &vox[VOX_PLAYER], "Voices-Player");
    voxOk[VOX_OPP] = LoadStream(songDir, "Voices-Opponent", &vox[VOX_OPP], "Voices-Opponent");

    ApplyVolumes();
    ApplySpeed();
}

void PlayStateAudio_Unload(void) {
    AudioStretch_Off(0, instOk ? &inst : NULL);   // processor sai antes do stream (o raylib nao libera)
    for (int i = 0; i < VOX_COUNT; i++)
        AudioStretch_Off(1 + i, voxOk[i] ? &vox[i] : NULL);

    if (instOk) UnloadMusicStream(inst);
    instOk = 0;
    for (int i = 0; i < VOX_COUNT; i++) {
        if (voxOk[i]) UnloadMusicStream(vox[i]);
        voxOk[i] = 0;
    }
}

void PlayStateAudio_Reset(void) {
    lastVoiceSync = -10.0f;
    wasPaused = 1;
}

// Faz as faixas carregadas passarem por uma acao (play/pause/resume/update).
typedef void (*TrackAction)(Music*);

static void ForEachTrack(TrackAction action) {
    if (instOk) action(&inst);
    for (int i = 0; i < VOX_COUNT; i++)
        if (voxOk[i]) action(&vox[i]);
}

static void PlayTrack(Music* m) { PlayMusicStream(*m); }
static void PauseTrack(Music* m) { PauseMusicStream(*m); }
static void ResumeTrack(Music* m) { ResumeMusicStream(*m); }
static void UpdateTrack(Music* m) { UpdateMusicStream(*m); }

void PlayStateAudio_StartPaused(void) {
    ForEachTrack(PlayTrack);
    ForEachTrack(PauseTrack);
    wasPaused = 1;
}

void PlayStateAudio_Pause(void) {
    ForEachTrack(PauseTrack);
    wasPaused = 1;
}

void PlayStateAudio_UpdateStreams(void) {
    ForEachTrack(UpdateTrack);
}

void PlayStateAudio_Update(void) {
    PlayStateAudio_UpdateStreams();

    if (wasPaused) {
        ForEachTrack(ResumeTrack);
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
    const float time = GetMusicTimePlayed(inst);
    if (time - lastVoiceSync <= 0.5f) return;

    char synced = 0;
    for (int i = 0; i < VOX_COUNT; i++) {
        if (voxOk[i] && fabsf(time - GetMusicTimePlayed(vox[i])) > 0.02f) {
            SeekMusicStream(vox[i], time);
            synced = 1;
        }
    }
    if (synced) lastVoiceSync = time;
}

/* Seek de todas as faixas juntas. Mantem o estado de pausa (quem pausou continua pausado). */
void PlayStateAudio_SeekTo(double seconds) {
    const float length = PlayStateAudio_Length();
    if (!instOk || length <= 0.0f || !isfinite(seconds)) return;

    const float time = (float)fmax(0.0, fmin(seconds, (double)length - 0.05));
    if (instOk) SeekMusicStream(inst, time);
    for (int i = 0; i < VOX_COUNT; i++)
        if (voxOk[i]) SeekMusicStream(vox[i], time);

    lastVoiceSync = time; // ja alinhadas: o SyncVoices so volta a checar depois de 0.5s
    if (speed != 1.0f) ApplySpeed(); // descarta o audio antigo guardado na Sonic
}

void PlayStateAudio_SetVolume(float volume) {
    appliedVolume = volume;
    ApplyVolumes();
}

Music* PlayStateAudio_Inst(void) {
    return &inst;
}
