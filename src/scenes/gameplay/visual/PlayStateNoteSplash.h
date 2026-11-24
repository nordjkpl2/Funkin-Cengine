/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSplash.h - splash de acerto (noteSplashes-sparkles) nos receptores do BF.
 *
 * Pool fixo por coluna (sem alocar no acerto). Cada acerto gera um splash novo.
 */
#pragma once
#ifndef PLAYSTATE_NOTESPLASH_H
#define PLAYSTATE_NOTESPLASH_H

/* Carrega a textura/atlas uma vez (entre as musicas). Chamar no Create. */
void PlayStateNoteSplash_Load(void);

/* Esvazia o pool. Chamar em todo Create, inclusive no restart. */
void PlayStateNoteSplash_Reset(void);

/* Splash na coluna (0..3) do receptor do BF. Sem cooldown: em spam, recicla o splash mais antigo da coluna. */
void PlayStateNoteSplash_Spawn(int lane);

/* Anima e desenha os splashes ativos. Chamar logo depois dos receptores e antes das notas. */
void PlayStateNoteSplash_Draw(float deltaTime);

#endif /* PLAYSTATE_NOTESPLASH_H */
