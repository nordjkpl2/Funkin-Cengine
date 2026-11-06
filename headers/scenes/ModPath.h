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

#endif /* MODPATH_H */
