/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAudio.h - instrumental e vozes do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_AUDIO_H
#define PLAYSTATE_AUDIO_H

#include "Render.h"

/* Quais faixas ficam audiveis (test play). As outras seguem tocando em volume 0. */
typedef enum PlayStateAudioMix {
    PSA_MIX_FULL = 0,   /* inst + vozes (Voices.ogg, ou o par Player/Opponent) */
    PSA_MIX_INST,       /* so o instrumental */
    PSA_MIX_VOCALS,     /* so as vozes (Voices.ogg, ou o par separado) */
    PSA_MIX_BF,         /* so Voices-Player */
    PSA_MIX_OPP         /* so Voices-Opponent */
} PlayStateAudioMix;

/* Chamar antes de Load (ou depois, a troca vale na hora). */
void PlayStateAudio_SetMix(PlayStateAudioMix mix);

/* Carrega Inst, Voices, Voices-Player e Voices-Opponent (ogg, mp3 ou wav) de songDir. Vozes sao opcionais. */
void PlayStateAudio_Load(const char* songDir);
void PlayStateAudio_Unload(void);

/* Chamar em todo Create: volta o estado de pausa e a sincronia das vozes. */
void PlayStateAudio_Reset(void);

/* Toca e pausa em seguida; o Update retoma quando a musica deve comecar. */
void PlayStateAudio_StartPaused(void);

void PlayStateAudio_Pause(void);

/* Atualiza os streams e retoma se estavam pausados. */
void PlayStateAudio_Update(void);

/* So atualiza os streams (aquecimento no Create). */
void PlayStateAudio_UpdateStreams(void);

char PlayStateAudio_IsPlaying(void);
float PlayStateAudio_Length(void);

/* Realinha as vozes ao instrumental (no maximo a cada 0.5s). */
void PlayStateAudio_SyncVoices(void);

void PlayStateAudio_SetVolume(float volume);

/* Instrumental para o BeatManager (songBeat.music). */
Music* PlayStateAudio_Inst(void);

#endif /* PLAYSTATE_AUDIO_H */
