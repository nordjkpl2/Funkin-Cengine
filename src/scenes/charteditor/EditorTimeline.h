/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: tempo do chart (inicio dos trechos, cursor, janela visivel). */
#pragma once
#ifndef EDITOR_TIMELINE_H
#define EDITOR_TIMELINE_H

#include "std.h"
#include "Render.h"
#include "scenes/Song.h"

extern int secIdx;
extern int snapDiv;
extern double cursorMs;
extern double viewTopMs;
void SecTimes_Drop(void);
void SecTimes_Invalidate(void);
double SecStartMs(int idx);
double SongEndMs(void);
int SecAtTime(double ms);
void SecRangeInView(double fromMs, double toMs, int* first, int* last);
void SecFollowCursor(void);

#endif
