/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl - Oct 19 2026 */
#pragma once
#ifndef WEEK_H
#define WEEK_H

#include "data/Song.h"
#define WEEK_MAX_SONGS 30

typedef struct {
    char song[64];
    char character[SONG_NAME_MAX];
} WeekSong;

typedef struct {
    char file[64];
    char title[64];
    char flavor[192];
    char dir[256];
    char fromMods;
    WeekSong songs[WEEK_MAX_SONGS];
    int songCount;
} WeekEntry;

int WeekList_Scan(WeekEntry** outEntries);
void WeekList_Free(WeekEntry* entries);
char Week_Delete(const WeekEntry* e);
void WeekRun_Start(const WeekEntry* e, int diff);
void WeekRun_Stop(void);
const char* WeekRun_Dir(void);
const char* WeekRun_Char(void);
const char* WeekRun_DiffSuffix(void);
int WeekRun_Advance(void);
void WeekRun_AddScore(int s);
int WeekRun_LastTotal(void);
const char* WeekRun_Missing(void);
void WeekRun_Finish(void);
#endif