/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.h - eventos do events.json tocados no PlayState (ex.: Fade In / Fade Out / Show Image).
 *
 * Formato Psych: {"events": [[tempo_ms, [[nome, v1, v2, v3, s1]]]]}.
 * Fade In: tela preta -> imagem. v1 = duracao (ms), v2 = alpha final (%).
 * Fade Out: imagem -> tela preta. v1 = duracao (ms), v2 = alpha final (%).
 * Show Image: mostra images/<s1>.png. v1 = x, v2 = y (px a partir do centro da tela, 0,0 = centralizado),
 *             v3 = escala (%). Fica visivel ate o proximo Show Image ou Hide Image.
 * Hide Image: esconde a imagem atual. Sem valores.
 */
#pragma once
#ifndef PLAYSTATE_EVENTS_IN_GAMEPLAY_H
#define PLAYSTATE_EVENTS_IN_GAMEPLAY_H

/* Tipos conhecidos pelo engine. Fonte unica: o editor de chart lista estes mesmos nomes no picker. */
typedef struct {
    const char* name;
    const char* description;
    int defaultDurMs;      /* v1 padrao */
    int defaultAlphaPct;   /* v2 padrao */
    int defaultScalePct;   /* v3 padrao (0 = tipo sem v3) */
    const char* label1;    /* rotulo do campo v1 no editor */
    const char* label2;    /* rotulo do campo v2 no editor */
    const char* label3;    /* rotulo do campo v3 no editor (NULL = sem v3) */
    char usesImage;        /* 1 = tem campo de texto com o nome da imagem (s1) */
    char usesShader;       /* 1 = evento de shader de tela: v1 = intensidade %, v2 = animacao % (0 = estatico) */
} PlayStateEventType;

extern const PlayStateEventType playStateEventTypes[];
extern const int playStateEventTypeCount;

/* Liga/desliga o shader de tela de um evento "Shader ..." (usado pelo play e pelo preview do editor).
 * intensityPct 0..100; animPct 0 = estatico, 100 = velocidade normal. */
void PlayStateEvents_ApplyShader(const char* name, double intensityPct, double animPct);

/* Le <songDir>/events.json (ausente = sem eventos) e carrega as imagens dos Show Image.
 * Chamar no Create, depois de saber a pasta. */
void PlayStateEvents_Load(const char* songDir);

/* Libera os eventos carregados e as texturas. */
void PlayStateEvents_Unload(void);

/* Volta ao inicio (restart). */
void PlayStateEvents_Reset(void);

/* Seek: dispara o estado que valeria em 'seconds' (sem reproduzir os fades intermediarios). */
void PlayStateEvents_SeekTo(double seconds);

/* Uma vez por frame com a musica tocando: dispara os eventos vencidos e atualiza o alpha. */
void PlayStateEvents_Update(double seconds);

/* Desenha a imagem do Show Image (se houver) e, por cima, o overlay preto do fade.
 * Chamar com a camera HUD aberta. */
void PlayStateEvents_Draw(void);

#endif /* PLAYSTATE_EVENTS_IN_GAMEPLAY_H */
