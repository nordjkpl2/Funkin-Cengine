/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * NoteTypes.h - tipos de nota (note[3] do chart da Psych).
 *
 * Registro de nomes -> indice (uint8, guardado em DataNote/Note/EdNote). Os 6 primeiros sao os da
 * Psych na ordem de Note.defaultNoteTypes (chart antigo guarda o numero). Customizados: o nome vem
 * do chart e as regras de custom_notetypes/<nome>.lua (so o subset setPropertyFromGroup do onCreate).
 * O registro so cresce (nunca reindexa): indices ficam validos entre musicas e threads.
 */
#pragma once
#ifndef NOTETYPES_H
#define NOTETYPES_H

#include <stdbool.h>

#define NOTETYPE_MAX 128
#define NOTETYPE_NAME_MAX 48

enum {
    NOTETYPE_NORMAL = 0,
    NOTETYPE_ALT,       /* "Alt Animation": canta com sufixo -alt */
    NOTETYPE_HEY,       /* "Hey!": canta e toca hey (GF: cheer) */
    NOTETYPE_HURT,      /* "Hurt Note": acertar = miss; deixar passar = nada */
    NOTETYPE_GF,        /* "GF Sing": a GF canta no lugar */
    NOTETYPE_NOANIM,    /* "No Animation": ninguem anima */
    NOTETYPE_BUILTIN
};

typedef struct {
    char name[NOTETYPE_NAME_MAX];
    char animSuffix[16];        /* somado a singLEFT... e singLEFTmiss... */
    char texture[64];           /* imagem relativa a images/ (Psych 'texture'); "" = skin da musica */
    char hitAnim[24];           /* goodNoteHit: characterPlayAnim(alvo, anim) */
    char missAnim[24];          /* noteMiss: idem */
    signed char hitAnimTarget;  /* 0 = oponente, 1 = BF, 2 = GF, -1 = nenhum */
    signed char missAnimTarget;
    float hitHealth;            /* escala do engine (0..1, metade da Psych); NAN = padrao */
    float missHealth;
    unsigned hitKills : 1;      /* goodNoteHit/noteMiss: setProperty('health', <= 0) */
    unsigned missKills : 1;
    unsigned hitCausesMiss : 1;
    unsigned ignorePlayer : 1;  /* lado do jogador: passar nao e miss; o bot nao aperta */
    unsigned ignoreOpponent : 1;/* lado do oponente: o oponente nao aperta */
    unsigned noAnimation : 1;
    unsigned noMissAnimation : 1;
    unsigned gfNote : 1;
    unsigned lowPriority : 1;   /* outra nota na mesma janela/coluna ganha o aperto */
    unsigned noSplash : 1;
    unsigned hey : 1;
    unsigned hurtAnim : 1;      /* BF toca "hurt" ao acertar (se tiver) */
    unsigned hurtColor : 1;     /* sem textura: desenha com a cor de nota de dano */
    unsigned scripted : 1;      /* regras vieram de custom_notetypes/<nome>.lua */
} NoteTypeInfo;

/* Indice do nome (cria se novo). "" = 0; registro cheio = 0. Seguro em thread de fundo. */
int NoteTypes_Intern(const char* name);
/* Numero do chart antigo (Psych 0.1-0.3 / Week 7): indice da lista padrao, fora dela = 0. */
int NoteTypes_FromNumber(double value);
/* -1 se o nome nao existe. */
int NoteTypes_Find(const char* name);
int NoteTypes_Count(void);
/* Fora do intervalo = normal. */
const char* NoteTypes_Name(int type);
const NoteTypeInfo* NoteTypes_Get(int type);

/* Rele custom_notetypes/<nome>.lua de cada tipo customizado (mod da musica, assets, mods ativos).
   Tipo sem script fica como nota normal. Chamar antes da gameplay, depois de ler o chart. */
void NoteTypes_LoadScripts(const char* songDir);
/* Editor: registra cada custom_notetypes/<nome>.lua achado (mesmas raizes). */
void NoteTypes_ScanScripts(const char* songDir);

#endif /* NOTETYPES_H */
