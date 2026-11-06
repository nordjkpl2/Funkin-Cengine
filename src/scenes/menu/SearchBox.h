/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SearchBox.h - campo de pesquisa com lupa, reaproveitavel por qualquer lista.
 *
 * Uso: SearchBox_Init uma vez; a cada frame SearchBox_Update (devolve true quando
 * o filtro deve ser aplicado: Enter ou clique na lupa); SearchBox_Draw por ultimo.
 * A tela filtra a propria lista com SearchBox_Match. Sem alocacao.
 */
#pragma once
#ifndef SEARCHBOX_H
#define SEARCHBOX_H

#include "raylib.h"
#include <stdbool.h>

#define SEARCH_TEXT_MAX 48

typedef struct {
    Rectangle bounds;              /* campo + lupa (coordenadas do mundo 1280x720) */
    char text[SEARCH_TEXT_MAX];    /* o que esta digitado */
    char applied[SEARCH_TEXT_MAX]; /* filtro em uso (ja normalizado) */
    int length;
    bool focused;
} SearchBox;

void SearchBox_Init(SearchBox* box, Rectangle bounds);

/* Teclado e mouse. Enquanto focado, consome o texto digitado. true = aplicar filtro. */
bool SearchBox_Update(SearchBox* box);

/* true quando o campo esta ativo: a tela deve ignorar atalhos de letras. */
bool SearchBox_Focused(const SearchBox* box);

/* O nome contem o filtro aplicado? Ignora maiusculas e trata - _ espaco como iguais. */
bool SearchBox_Match(const SearchBox* box, const char* name);

void SearchBox_Draw(const SearchBox* box, Font font);

#endif /* SEARCHBOX_H */
