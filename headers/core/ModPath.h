#pragma once
#ifndef MODPATH_H
#define MODPATH_H

#include <stdbool.h>
#include <stddef.h>

/* Raiz do mod de uma pasta de musica. Aceita os dois formatos:
 *   assets/mods/<mod>/songs/<musica>  ->  assets/mods/<mod>
 *   assets/mods/songs/<musica>        ->  assets/mods   (mod importado direto)
 * Barras invertidas sao normalizadas. Devolve false se nao for um mod. */
bool ModPath_Root(const char* restrict songDir, char* restrict out, size_t capacity);

/* Procura <root>/<relative> numa unica raiz (sem fallback). Para ordens de busca
 * proprias. root vazio ou NULL = false. Devolve true com o caminho em out. */
bool ModPath_FindIn(const char* restrict root, const char* restrict relative,
                    char* restrict out, size_t capacity);

/* Acha um arquivo: <modRoot>/<relative> primeiro, depois assets/<relative>.
 * modRoot pode ser NULL ou vazio (so assets). Ex.: relative = "images/characters/bf.png".
 * Devolve true com o caminho em out; false com out vazio. Sem estado global. */
bool ModPath_Find(const char* restrict modRoot, const char* restrict relative,
                  char* restrict out, size_t capacity);

#endif /* MODPATH_H */
