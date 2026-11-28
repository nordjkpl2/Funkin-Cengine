/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateEventsInGameplay.h - eventos do events.json tocados no PlayState (ex.: Fade In / Fade Out / Show Image).
 *
 * Formato Psych: {"events": [[tempo_ms, [[nome, v1, v2, v3, s1]]]]} (tambem {"song": {"events": ...}},
 * "events" dentro do chart e notas legadas de casa -1; ver data/song/SongEvents.h).
 * Fade In: tela preta -> imagem. v1 = duracao (ms), v2 = alpha final (%).
 * Fade Out: imagem -> tela preta. v1 = duracao (ms), v2 = alpha final (%).
 * Show Image: mostra images/<s1>.png. v1 = x, v2 = y (px a partir do centro da tela, 0,0 = centralizado),
 *             v3 = escala (%). Fica visivel ate o proximo Show Image ou Hide Image.
 * Hide Image: esconde a imagem atual. Sem valores.
 * Change Character: v1 = alvo (0 = oponente, 1 = BF, 2 = GF); s1 = nome do personagem (pasta do mod/assets).
 *                   Formato Psych [nome, alvo, personagem] (alvo bf/dad/gf ou 0 = BF, 1 = oponente, 2 = GF) tambem vale.
 * Letterbox: barras pretas em cima e embaixo. v1 = duracao da animacao (ms), v2 = altura de cada barra (% da tela; 0 tira).
 * Music Credit: disco de vinil grande com o titulo da musica (nome da pasta) e o criador. v1 = duracao (ms), s1 = nome do criador.
 *
 * Eventos padrao da Psych (v1/v2 em texto, como na Psych): Hey!, Set GF Speed, Add Camera Zoom, Play Animation,
 * Camera Follow Pos, Alt Idle Animation, Screen Shake, Change Scroll Speed, Set Property (subconjunto), Play Sound.
 * Do JS Engine: Flash Camera, Set Camera Zoom, Credits Popup, Camera Bopping. Eventos de stage da Psych
 * (Philly Glow, Kill Henchmen...) sao ignorados; nomes desconhecidos geram um aviso no log ao carregar.
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
    char usesChar;         /* 1 = troca de personagem: v1 = alvo (0 = oponente, 1 = BF, 2 = GF), s1 = nome do personagem */
    char usesText;         /* 1 = so texto livre em s1 (ex.: nome do criador); sem v2 */
    char textValues;       /* 1 = estilo Psych: v1 e v2 sao texto livre (vazio = padrao do evento), sem v3/s1 */
    const char* hint1;     /* textValues: exemplo mostrado no campo vazio */
    const char* hint2;
} PlayStateEventType;

extern const PlayStateEventType playStateEventTypes[];
extern const int playStateEventTypeCount;

/* Liga/desliga o shader de tela de um evento "Shader ..." (usado pelo play e pelo preview do editor).
 * intensityPct 0..100; animPct 0 = estatico, 100 = velocidade normal. */
void PlayStateEvents_ApplyShader(const char* name, double intensityPct, double animPct);

/* Le <songDir>/events.json, os eventos do chart (diff "" = padrao) e carrega imagens/sons usados.
 * Chamar no Create, depois de saber a pasta. */
void PlayStateEvents_Load(const char* songDir, const char* diff);

/* Libera os eventos carregados, texturas e sons. */
void PlayStateEvents_Unload(void);

/* Volta ao inicio (restart). */
void PlayStateEvents_Reset(void);

/* Seek: dispara o estado que valeria em 'seconds' (sem reproduzir os fades intermediarios). */
void PlayStateEvents_SeekTo(double seconds);

/* Uma vez por frame com a musica tocando: dispara os eventos vencidos e atualiza o alpha. */
void PlayStateEvents_Update(double seconds);

/* Multiplicador da velocidade das notas (Change Scroll Speed; 1 = a do chart). */
float PlayStateEvents_ScrollMult(void);

/* Set Property camHUD / camGame .visible/.alpha: 0 = nao desenhar o HUD / o mundo. */
char PlayStateEvents_HudVisible(void);
char PlayStateEvents_WorldVisible(void);

/* Desenha a imagem do Show Image (se houver), o overlay preto do fade e o Flash Camera.
 * Chamar com a camera HUD aberta. */
void PlayStateEvents_Draw(void);

/* Barras do Letterbox. Chamar com a camera HUD aberta, antes do HUD (notas ficam por cima). */
void PlayStateEvents_DrawLetterbox(void);

#endif /* PLAYSTATE_EVENTS_IN_GAMEPLAY_H */
