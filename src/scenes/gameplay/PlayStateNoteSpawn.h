/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSpawn.h - notas da musica: carregar, atualizar e desenhar.
 *
 * Cuida da lista de notas, do culling e do desenho (cabeca, sustain e fim).
 * O que mexe em personagem, score ou vida fica no PlayState e entra por callbacks.
 */
#pragma once
#ifndef PLAYSTATE_NOTESPAWN_H
#define PLAYSTATE_NOTESPAWN_H

#include "scenes/AllScenes.h"
#include "logic/Note.h"
#include "data/Song.h"

typedef struct {
    void (*opponentHit)(Note* note);            /* oponente acertou a nota */
    void (*botHit)(Note* note);                 /* botplay acertou a nota do BF */
    void (*opponentHold)(void);                 /* oponente segurando sustain */
    void (*botHold)(void);                      /* botplay segurando sustain */
    void (*playerHold)(void);                   /* jogador segurando sustain certo */
    void (*miss)(Note* note);                   /* nota perdida ou sustain solto */
} PlayStateNoteEvents;

typedef struct {
    float time;                 /* songBeat.time */
    float songSpeed;            /* song.speed */
    float scrollVelocity;       /* +450 downscroll, -450 nos demais */
    char paused;
    const StrumNote* dadStrums; /* 4 receptores do oponente */
    const StrumNote* bfStrums;  /* 4 receptores do BF */
} PlayStateNoteView;

/* Uma vez por cena: prepara os objetos de desenho (cabeca, sustain, fim). */
void PlayStateNoteSpawn_Init(void);

void PlayStateNoteSpawn_SetEvents(const PlayStateNoteEvents* events);

/* Monta e ordena a lista de notas a partir da musica. */
void PlayStateNoteSpawn_Load(const Song* song);

/* Restart: volta o culling e limpa pressed/missed. */
void PlayStateNoteSpawn_Reset(void);

void PlayStateNoteSpawn_Unload(void);

/* Acesso para o input (NoteHits). */
Note* PlayStateNoteSpawn_Notes(void);
size_t PlayStateNoteSpawn_Count(void);
size_t PlayStateNoteSpawn_First(void);

/* Culling + hits automaticos + desenho. Chamar dentro da camera do HUD. */
void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view);

#endif /* PLAYSTATE_NOTESPAWN_H */
