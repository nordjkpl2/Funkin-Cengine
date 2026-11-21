/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl, outd day19 of10 2026 */
//!OUTD day19/10/2026
#pragma once
#ifndef SONG_H
#define SONG_H
#include <stdint.h>

/* Tamanho (com o \0) de nomes de personagem, stage e skin. Cabe nome de mod longo. */
#define SONG_NAME_MAX 64
typedef struct {
    float time;
    int16_t id;
    int16_t _pad;
    float len;
} DataNote;
typedef struct {
    char mustHit;
    float bpm; 
    int len; 
    DataNote* notes;
    int noteCount;
} Section;
typedef struct {
    char player1[SONG_NAME_MAX];
    char player2[SONG_NAME_MAX];
    char stage[SONG_NAME_MAX];
    char gfVersion[SONG_NAME_MAX];
    char noteSkin[SONG_NAME_MAX];
    char arrowSkin[SONG_NAME_MAX];   /* Psych: imagem da skin relativa a images/, sem extensao */
    Section* sections;
    int sectionCount;
    float bpm;
    float speed;
    /* Modo streaming (chart JSON): as notas ficam no arquivo e a gameplay le por secao.
       Quando nao NULL, sections[].notes estao vazias e o dono do stream e quem recebe o Song
       (PlayStateNoteSpawn). Ver data/SongStream.h. */
    struct SongStream* stream;
} Song;
void Song_Parse(Song* song, const char* songDataPath);
/* streaming = 1: chart JSON em janelas por secao quando possivel; senao carrega inteiro. */
void Song_LoadSongFromDir(Song* song, const char* dirPath, char streaming);
void Song_LoadSongFromDirDiff(Song* song, const char* dirPath, const char* diff, char streaming);
char Song_HasChart(const char* dirPath);
void Song_Free(Song* song);
/* Libera so as notas crus de cada secao (ja copiadas para o array de notas da gameplay).
   Mantem sectionCount, bpm, mustHit e len, que o beat ainda usa. */
void Song_ReleaseNotes(Song* song);
#endif