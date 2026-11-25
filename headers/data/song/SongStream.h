/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * SongStream.h - chart JSON em modo streaming ("janelas por secao").
 *
 * Abrir o chart indexa as secoes (inicio no tempo, maior fim de nota, bpm, mustHit) sem manter
 * as notas na RAM. Durante a musica, a gameplay pede so as secoes perto do tempo atual.
 * Retorna NULL quando nao da para usar o modo streaming; quem chama carrega o chart inteiro.
 */
#pragma once
#ifndef SONGSTREAM_H
#define SONGSTREAM_H

#include "data/song/Song.h"

typedef struct SongStream SongStream;

/* Indexa o chart. Preenche song->sections (meta, sem notas) e song->bpm/speed/nomes.
 * NULL se: arquivo ilegivel, JSON fora do formato, ou notas fora de ordem entre secoes
 * (o modo streaming assume secoes em ordem de tempo). Em caso de NULL, song nao muda. */
SongStream *SongStream_Open(const char *jsonPath, Song *song);
void SongStream_Close(SongStream *stream);

int SongStream_SectionCount(const SongStream *stream);
/* Tempo (ms) a partir do qual a secao k precisa estar carregada: menor entre o inicio e a primeira nota
   (com sufixo-minimo, o que garante que nenhuma nota e pedida antes de estar carregada). */
float SongStream_SectionLoadMs(const SongStream *stream, int k);
float SongStream_SectionMaxEndMs(const SongStream *stream, int k);

/* Le e converte as notas da secao k (tempos em ms do chart, como DataNote).
 * *out e liberado com free (NULL se a secao nao tem notas); retorna a quantidade. */
int SongStream_LoadSection(const SongStream *stream, int k, DataNote **out, char *mustHit);

#endif /* SONGSTREAM_H */
