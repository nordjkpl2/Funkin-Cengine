/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
#pragma once
#ifndef TRANSITION_H
#define TRANSITION_H

/* Transicao leve de cena: um veu preto que some (FadeIn) ou aparece (FadeOut).
 * Desenhe Transition_Draw() por ultimo no frame; ela avanca o proprio tempo. */
void Transition_FadeIn(float secs);   /* preto -> transparente */
void Transition_FadeOut(float secs);  /* transparente -> preto */
bool Transition_Done(void);           /* true quando o FadeOut terminou (ou nao ha transicao) */
void Transition_Draw(void);

#endif
