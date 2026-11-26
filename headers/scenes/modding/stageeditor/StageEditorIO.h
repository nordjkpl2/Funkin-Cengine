/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Stage Editor: lista, carregamento e salvamento de stages (<raiz>/stages/<nome>.lua + .json). */
#pragma once
#ifndef STAGEEDITORIO_H
#define STAGEEDITORIO_H

#include <stdbool.h>
#include <stddef.h>
#include "logic/stage/Stage.h"
#include "scenes/modding/ModdingCommon.h"

#define STAGEED_LIST_MAX 256

typedef struct {
    char name[MODED_NAME_MAX];
    char root[MODED_ROOT_MAX]; /* raiz onde o stage mora ("assets" ou a pasta do mod) */
} StageEdEntry;

/* Stages dos mods ativos e de assets (mesma ordem de busca do jogo). Devolve a quantidade. */
int StageEd_ScanList(StageEdEntry *out, int capacity);

/* Imagens disponiveis em <raiz>/images/ e um nivel de subpasta ("pasta/nome"), sem extensao. */
int StageEd_ScanImages(char out[][MODED_NAME_MAX], int capacity);

/* Nome de imagem aceito no .lua: letras, digitos, - _ . espaco e / (sem .. nem / inicial). */
bool StageEd_IsImageName(const char *image);

/* Caminho da imagem como o jogo procura: <raiz>/images/<nome>.png, senao assets/images. */
bool StageEd_ImagePath(const char *root, const char *image, char *out, size_t capacity);

/* Le o .lua (sprites) e o .json (meta). Sempre preenche meta com padroes. */
bool StageEd_Load(const StageEdEntry *entry, StageSprite *sprites, int capacity, int *count,
                  StageMeta *meta, char *msg, size_t msgCap);

/* Salva o .lua (gerado; o original vai para .lua.bak na 1a vez) e atualiza o .json. */
bool StageEd_Save(const StageEdEntry *entry, const StageSprite *sprites, int count,
                  const StageMeta *meta, char *msg, size_t msgCap);

#endif /* STAGEEDITORIO_H */
