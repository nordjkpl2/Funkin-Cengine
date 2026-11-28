/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateChangers.h - Gameplay Changers da Psych ligados as notas (painel "> Notes" da pausa).
 *
 * Scroll Type: Multiplicative = velocidade do chart x valor (eventos Change Scroll Speed valem);
 * Constant = valor fixo (ignora o chart e os eventos). Instakill on Miss: um miss mata.
 * Ficam no restart; PlayState volta ao padrao em musica nova (como Song speed e Health Multiplier).
 */
#pragma once
#ifndef PLAYSTATE_CHANGERS_H
#define PLAYSTATE_CHANGERS_H

typedef enum { SCROLL_TYPE_MULT, SCROLL_TYPE_CONST, SCROLL_TYPE_COUNT } ScrollType;

#define CHANGERS_SCROLL_STEP 0.05f
#define CHANGERS_SCROLL_MIN 0.35f
#define CHANGERS_SCROLL_MAX(type) ((type) == SCROLL_TYPE_CONST ? 6.0f : 3.0f)

/* Padrao: Multiplicative 1x, Instakill off. */
void PlayStateChangers_Reset(void);

ScrollType PlayStateChangers_ScrollType(void);
float PlayStateChangers_ScrollValue(void);
char PlayStateChangers_Instakill(void);

/* Retornam 1 se mudou. O valor e limitado ao intervalo do tipo e arredondado ao passo. */
char PlayStateChangers_SetScrollType(ScrollType type);
char PlayStateChangers_SetScrollValue(float value);
char PlayStateChangers_SetInstakill(char on);

/* 1 se algo esta fora do padrao. */
char PlayStateChangers_IsModified(void);

/* Velocidade final das notas: chartSpeed = song.speed, eventMult = Change Scroll Speed. */
float PlayStateChangers_SongSpeed(float chartSpeed, float eventMult);

#endif /* PLAYSTATE_CHANGERS_H */
