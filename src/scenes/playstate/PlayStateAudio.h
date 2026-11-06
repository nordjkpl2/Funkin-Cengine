/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAudio.h - instrumental e vozes do PlayState.
 */
#pragma once
#ifndef PLAYSTATE_AUDIO_H
#define PLAYSTATE_AUDIO_H

#include "scenes/AllScenes.h"

/* Carrega Inst e Voices (ogg, mp3 ou wav) de songDir. Voices e opcional. */
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
