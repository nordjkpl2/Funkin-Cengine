/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateText.h - textos do HUD: tempo, titulo, score, misses e BOTPLAY.
 */
#pragma once
#ifndef PLAYSTATE_TEXT_H
#define PLAYSTATE_TEXT_H


/* Carrega a fonte e prepara o titulo (nome da pasta de songDir). Uma vez por cena. */
void PlayStateText_Init(const char* songDir);
void PlayStateText_Unload(void);

/* Em todo Create: forca o texto a ser refeito. */
void PlayStateText_Reset(void);

/* Tempo, titulo, score e misses. songTime < 0 conta como 0:00. */
void PlayStateText_Draw(int score, int misses, float songTime, char showScore);

/* Marca d'agua fixa; desenhar por ultimo. */
void PlayStateText_DrawBotplayMark(void);

#endif /* PLAYSTATE_TEXT_H */
