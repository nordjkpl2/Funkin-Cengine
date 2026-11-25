/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl - Note.h Optimized, outd day19 of10 2026 */
#pragma once
#ifndef NOTE_H
#define NOTE_H

#include <stdint.h>
#include "Render.h"
#include "data/song/Song.h"

typedef struct {
    RayAnimatedObject object;
    int id;
} StrumNote;

/* 12 bytes: os flags ficam em bits (0/1) e a lane (-1..3) cabe em int8. */
typedef struct {
    float time;
    float length;
    int8_t id;                          /* lane 0..3, -1 = invalida */
    unsigned char mustHit : 1;
    unsigned char pressed : 1;
    unsigned char missed : 1;
    unsigned char sectionMustHit : 1;   /* quem canta na secao desta nota (mustHit da secao); usado por Hidden Notes */
} Note;

/* // * Animações */
void StrumNote_Load(StrumNote* note, int id);
int StrumNote_IdleAnimation(int id);
int StrumNote_PressAnimation(int id);
int StrumNote_ConfirmAnimation(int id);

void Note_Load(Note* note, DataNote* dataNote);
void Note_SetCurrentSkin(const RayAnimationHandler* skin);
const RayAnimationHandler* Note_GetCurrentSkin(void);

void Note_SetWindow(float w);

/* // * Hit/miss: a janela vem de Note_SetWindow (PlayState escolhe pela dificuldade) */
char Note_CanBeHit(Note* note, float time);
char Note_TooEarly(Note* note, float time);
char Note_TooLate(Note* note, float time);
char Note_ShouldHold(Note* note, float time); 

int Note_Animation(int id);
int Note_TrailAnimation(int id);
int Note_EndAnimation(int id);

#endif