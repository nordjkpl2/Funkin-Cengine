/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterInternal.h - Constantes, contexto de busca e helpers compartilhados
 * pelos modulos Character*.c (uso interno da logica de personagem).
 */
#pragma once
#ifndef CHARACTER_INTERNAL_H
#define CHARACTER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include "characters/Character.h"

#define CHAR_PATH_MAX       512
#define CHAR_MAX_DEFS       48
#define CHAR_JSON_MAX_SIZE  (1024 * 1024)
#define CHAR_XML_STACK_SIZE 2048u
#define CHAR_XML_READ_SIZE  4096u

/* Onde procurar: pasta do mod ("" = so assets/). Passado por parametro para
   o carregamento poder rodar fora da thread principal no futuro. */
typedef struct {
    char root[CHAR_PATH_MAX];
} CharCtx;

/* Contexto de busca: pasta do mod da musica (ou "" para so assets/). */
bool Char_Find(const CharCtx *ctx, char out[CHAR_PATH_MAX],
               const char *fmt, ...) __attribute__((format(printf, 3, 4)));
bool Char_FindPair(const CharCtx *ctx, char setOut[CHAR_PATH_MAX], char pngOut[CHAR_PATH_MAX],
                   const char *setName, const char *pngName);
void Char_ResolveCtx(CharCtx *out, const char *name);
void Char_StemName(const char *source, char *out, size_t capacity);
void Char_CopyString(char *out, size_t capacity, const char *source);
bool Char_IsSafeName(const char *name);
bool Char_SkipName(const char *const names[2], int i);
void Char_PsychBox(const char *pngPath, float scale, Vector2 *box, Vector2 *shift);
void Char_LoadIcon(const CharCtx *ctx, Character *character,
                   const char *name, const char *healthIcon, CharRole role);

#endif /* CHARACTER_INTERNAL_H */
