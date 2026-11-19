/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Character Editor: lista, carregamento e salvamento (JSON Psych) dos personagens. */
#pragma once
#ifndef CHAREDITORIO_H
#define CHAREDITORIO_H

#include <stdbool.h>
#include <stddef.h>
#include "characters/Character.h"

#define CHARED_NAME_MAX 64
#define CHARED_ROOT_MAX 256
#define CHARED_LIST_MAX 256

/* Personagem listado: nome e a raiz onde ele mora ("assets" ou a pasta do mod). */
typedef struct {
    char name[CHARED_NAME_MAX];
    char root[CHARED_ROOT_MAX];
} CharEdEntry;

/* Lista os personagens (images/characters/<nome>.animset + .png) dos mods ativos e de assets.
   Mods vem antes de assets, na mesma ordem que o carregamento usa. Devolve a quantidade. */
int  CharEd_ScanList(CharEdEntry *out, int capacity);

/* Carrega o personagem como o jogo faria (Character_Load). */
bool CharEd_LoadCharacter(const CharEdEntry *entry, Character *character);
void CharEd_FreeCharacter(Character *character);

/* Salva <raiz>/characters/<nome>.json. Se o arquivo ja existe, so atualiza os campos que o
   editor controla (escala, flip, posicao, sing_duration, offsets e loop das animacoes): o resto
   do JSON fica como estava. msg recebe um resumo para a barra de status. */
bool CharEd_Save(const CharEdEntry *entry, const Character *character, char *msg, size_t msgCap);

#endif /* CHAREDITORIO_H */
