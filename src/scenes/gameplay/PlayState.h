/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef PLAYSTATE_H
#define PLAYSTATE_H

void PlayState_SetScene(void);
void PlayState_SetSongDir(const char* dir);
void PlayState_SetSongDirKeep(const char* dir);
void PlayState_SetReturnEditor(char on);
void PlayState_SetReturnWeek(char on);
void PlayState_SetDiff(const char* diff);
void PlayState_SetCharsOverride(const char* p2);

#endif
