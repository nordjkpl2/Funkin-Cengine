/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef SCENES_MENU_MENUMUSIC_H
#define SCENES_MENU_MENUMUSIC_H

/* Musicas de menu: assets/music/menu/ (arquivos .ogg). Nome do arquivo "Titulo_by_Autor" vira "Titulo by Autor". */
#define MENU_MUSIC_DEFAULT "Alpha_by_BambleW"

/* Escaneia a pasta e toca a faixa salva (ou a padrao). Chamar uma vez no inicio do jogo, depois do GameData. */
void MenuMusic_Init(void);

int         MenuMusic_Count(void);
const char* MenuMusic_Label(int index);     /* "Titulo by Autor" */
const char* MenuMusic_FileName(int index);  /* nome sem extensao (o que e salvo) */
int         MenuMusic_SavedIndex(void);     /* indice da faixa salva (padrao se nao achar) */

void MenuMusic_Play(int index);             /* troca a musica de menu (nao salva) */
void MenuMusic_RestoreSaved(void);          /* volta para a faixa salva */

#endif
