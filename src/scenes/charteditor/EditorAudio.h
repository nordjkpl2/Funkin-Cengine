/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: audio do editor (Inst, Voices e vozes separadas andam juntos). */
#pragma once
#ifndef EDITOR_AUDIO_H
#define EDITOR_AUDIO_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"

extern char playing;
extern char playFollow;
extern float edVolApplied;
extern Music edInst;
extern char edHasAudio;
extern Music edVoices;
extern char edHasVoices;
extern Music edSplitO;
extern Music edSplitP;
extern char edHasSplit;
void Audio_SetVolume(float v);
void Audio_Update(void);
void Audio_SeekAll(double ms);
void Audio_StopPlay(void);
void Audio_Toggle(void);

#endif
