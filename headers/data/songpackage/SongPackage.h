/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * SongPackage.h - exporta uma musica e tudo que ela usa para uma pasta de mod.
 *
 * Saida: exports/<musica>/ no layout de mod (songs/, characters/, images/, stages/...) com o
 * marcador SONG_PACKAGE_MARKER. O import (Modding > Import) copia a pasta como esta.
 * So copia arquivos da raiz do mod da musica (e de mods ativos que tenham o personagem):
 * o que vem de assets/ base ja existe em qualquer cengine. Musica fora de mod usa assets/.
 */
#pragma once
#ifndef SONGPACKAGE_H
#define SONGPACKAGE_H

#include <stddef.h>

#define SONG_PACKAGE_MARKER   "cengine-package.json"
#define SONG_PACKAGE_DIR      "exports"
#define SONG_PACKAGE_MAX      32
#define SONG_PACKAGE_NAME_MAX 64

typedef struct SongPackageReq {
    const char *songDir;  /* pasta da musica (obrigatorio) */
    const char *stage;    /* "" = sem stage */
    const char *noteSkin; /* "" = skin padrao; "@rel" = caminho relativo a images/ */
    char characters[SONG_PACKAGE_MAX][SONG_PACKAGE_NAME_MAX]; /* p1, p2, gf, Change Character */
    int characterCount;
    char images[SONG_PACKAGE_MAX][SONG_PACKAGE_NAME_MAX];     /* Show Image (relativo a images/) */
    int imageCount;
} SongPackageReq;

/* Sem repetir; vazio ou lista cheia = ignorado. */
void SongPackage_AddCharacter(SongPackageReq *req, const char *name);
void SongPackage_AddImage(SongPackageReq *req, const char *name);

/* Sincrono. 1 = ok. msg recebe o resumo ou o erro. */
char SongPackage_Export(const SongPackageReq *req, char *msg, size_t msgSize);

/* 1 se dir e um pacote exportado (tem o marcador). */
char SongPackage_IsPackage(const char *dir);

#endif /* SONGPACKAGE_H */
