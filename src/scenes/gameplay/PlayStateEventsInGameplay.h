/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.h - eventos do events.json tocados no PlayState (ex.: Fade In / Fade Out).
 *
 * Formato Psych: {"events": [[tempo_ms, [[nome, v1, v2]]]]}.
 * Fade In: tela preta -> imagem. v1 = duracao (ms), v2 = alpha final (%).
 * Fade Out: imagem -> tela preta. v1 = duracao (ms), v2 = alpha final (%).
 */
#pragma once
#ifndef PLAYSTATE_EVENTS_IN_GAMEPLAY_H
#define PLAYSTATE_EVENTS_IN_GAMEPLAY_H

/* Tipos conhecidos pelo engine. Fonte unica: o editor de chart lista estes mesmos nomes no picker. */
typedef struct {
    const char* name;
    const char* description;
    int defaultDurMs;
    int defaultAlphaPct;
} PlayStateEventType;

extern const PlayStateEventType playStateEventTypes[];
extern const int playStateEventTypeCount;

/* Le <songDir>/events.json (ausente = sem eventos). Chamar no Create, depois de saber a pasta. */
void PlayStateEvents_Load(const char* songDir);

/* Libera os eventos carregados. */
void PlayStateEvents_Unload(void);

/* Volta ao inicio (restart). */
void PlayStateEvents_Reset(void);

/* Seek: dispara o estado que valeria em 'seconds' (sem reproduzir os fades intermediarios). */
void PlayStateEvents_SeekTo(double seconds);

/* Uma vez por frame com a musica tocando: dispara os eventos vencidos e atualiza o alpha. */
void PlayStateEvents_Update(double seconds);

/* Desenha o overlay preto do fade (se houver). Chamar com a camera HUD aberta. */
void PlayStateEvents_Draw(void);

#endif /* PLAYSTATE_EVENTS_IN_GAMEPLAY_H */
