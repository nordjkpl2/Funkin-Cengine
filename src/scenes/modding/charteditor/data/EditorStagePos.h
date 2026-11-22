#pragma once
#ifndef EDITOR_STAGE_POS_H
#define EDITOR_STAGE_POS_H

/* Chart Editor: posicao de boyfriend / dad / gf no <stage>.json (o jogo le o mesmo arquivo).
 * Edita o json do mod da musica; sem ele, o global (assets/stages). Stage so em .lua: sem edicao. */

/* Desenha a secao em (x, y) e devolve a altura usada. */
float StagePos_Draw(float x, float y, const char* stage, const char* songDir, char blocked);

#endif /* EDITOR_STAGE_POS_H */
