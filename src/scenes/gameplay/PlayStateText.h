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

/* Atualiza tempo, score e misses e rasteriza so o que mudou. Chamar no Update, NUNCA no Draw
   (usa render para textura, que nao pode aninhar com o render target da cena). songTime < 0 conta como 0:00.
   scorePosition: SCORE_POS_CENGINE (canto direito) ou SCORE_POS_PSYCH (centro sob a barra). */
void PlayStateText_Refresh(int score, int misses, float songTime, char showScore, char scorePosition);

/* Blit dos textos do HUD (tempo, titulo, score/misses). Dentro da camera do HUD. */
void PlayStateText_Draw(char showScore, char scorePosition);

/* Marca d'agua fixa; desenhar por ultimo. */
void PlayStateText_DrawBotplayMark(void);

#endif /* PLAYSTATE_TEXT_H */
