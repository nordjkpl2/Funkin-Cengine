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


#include "logic/notes/Note.h"
#include "data/song/Song.h"

typedef struct {
    void (*opponentHit)(Note* note);            /* oponente acertou a nota */
    void (*botHit)(Note* note);                 /* botplay acertou a nota do BF */
    void (*opponentHold)(const Note* note);     /* oponente segurando sustain */
    void (*botHold)(const Note* note);          /* botplay segurando sustain */
    void (*playerHold)(const Note* note);                   /* jogador segurando sustain certo */
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

/* Tipos de nota da musica: le custom_notetypes e acha a textura de cada tipo. Depois da skin da musica
   (Note_SetCurrentSkin) e antes de PlayStateNoteSpawn_Load. */
void PlayStateNoteSpawn_LoadTypes(const char* songDir);

/* Monta e ordena a lista de notas a partir da musica.
   Se song->stream existe (modo streaming), a gameplay assume o stream: as notas entram em janelas
   por secao durante o Update, e song->stream vira NULL. Os acessos Notes/Count/First sao sempre
   relativos a janela carregada. */
void PlayStateNoteSpawn_Load(Song* song);

/* Restart: volta o culling e limpa pressed/missed. */
void PlayStateNoteSpawn_Reset(void);

/* Seek para `time` (segundos): notas anteriores viram resolvidas sem pontuar; firstNote e reposicionado. */
void PlayStateNoteSpawn_SeekTo(float time);

void PlayStateNoteSpawn_Unload(void);

/* Acesso para o input (NoteHits). */
Note* PlayStateNoteSpawn_Notes(void);
size_t PlayStateNoteSpawn_Count(void);
size_t PlayStateNoteSpawn_First(void);

/* Logica: culling, hits do oponente/botplay, hold e miss. Chamar no Update, depois do input. */
void PlayStateNoteSpawn_Update(const PlayStateNoteView* view);

/* So desenho (le o estado). Chamar dentro da camera do HUD. */
void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view);

/* Menu de opcoes: nota desenhada com a cor da coluna (tint branco = skin original). */
void PlayStateNoteSpawn_DrawShifted(RayAnimatedObject* obj, Vector3 shift);
void PlayStateNoteSpawn_ReleaseTint(void);
/* Fora da gameplay (chart editor): liga o shader de matiz da coluna ate EndShift. Shift (0,0,0) = nada. */
void PlayStateNoteSpawn_BeginShift(Vector3 shift);
void PlayStateNoteSpawn_EndShift(void);

#endif /* PLAYSTATE_NOTESPAWN_H */
