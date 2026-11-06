/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef ALLSCENES_H
#define ALLSCENES_H

#include "Render.h"

/* // * code */
void PlayState_SetScene(void);
void PlayState_SetSongDir(const char* dir);
void PlayState_SetSongDirKeep(const char* dir);
void PlayState_SetReturnEditor(char on);
void PlayState_SetReturnWeek(char on);
void PlayState_SetDiff(const char* diff);
void PlayState_SetCharsOverride(const char* p2);
int PlayState_GetScore(void);
void LoadingState_SetScene(void);
void LoadingState_SetSongDir(const char* dir);
void ModdingMenu_SetScene(void);
void ChartEditor_SetScene(void);
void ChartEditor_SetSongDir(const char* dir);
void StoryMode_SetScene(void);
void OptionsMenu_SetScene(void);
void Freeplay_SetScene(void); 
void MenuState_SetScene(void); 
void EnterToPlay_SetScene(void);

extern Vector2 VECTOR_ZERO;
extern Font mainFont;  

void AllScenes_StartGame(void);
void AllScenes_DestroyGame(void);

RayAnimationHandler Cache_GetNoteAnimations(void);
RayAnimationHandler Cache_GetNoteSkin(const char* skinName, const char* songDir);
RayAnimationHandler* Cache_GetDefaultNoteSkinPtr(void);
void Cache_UnloadCustomNoteSkins(void);

#endif
