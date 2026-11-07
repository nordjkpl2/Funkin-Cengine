/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: listas de personagens, stages e noteskins (varredura de pastas) e as caixas ligadas a elas. */
#pragma once
#ifndef EDITOR_CHARACTERS_AND_LISTS_H
#define EDITOR_CHARACTERS_AND_LISTS_H

#include "std.h"
#include "Render.h"
#include "data/Song.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"
#include "EditorAudio.h"
#include "EditorWidgets.h"
#include "EditorGrid.h"

// lista de cantores (artes que existem): pro seletor Boyfriend/Opponent
#define NAME_LIST_MAX 1024  /* mods grandes tem centenas de personagens */

extern SongEntry* pickerSongs;
extern int pickerCount;
extern char charNames[NAME_LIST_MAX][SONG_NAME_MAX];
extern int charCount;
extern char gfNames[NAME_LIST_MAX][SONG_NAME_MAX];
extern int gfCount;
extern char stageNames[32][SONG_NAME_MAX];
extern int stageCount;
void EdModRoot(const char* songDir, char out[256]);
char CharacterAssetForSong_Exists(const char* name, const char* songDir);
char GirlfriendAsset_Exists(const char* name, const char* songDir);
char StageFileForSong_Exists(const char* name, const char* songDir);
char StageAsset_Exists(const char* name);
void StageList_Scan(void);
void CharList_Scan(void);
#define SKIN_LIST_MAX 128
extern char skinNames[SKIN_LIST_MAX][SONG_NAME_MAX];
extern int skinCount;
extern TxtBox skinBox;
void NoteSkinList_Scan(void);
extern TxtBox p1Box;
extern TxtBox p2Box;
extern TxtBox gfBox;
extern TxtBox stageBox;

#endif
