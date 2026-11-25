/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ShaderFx.h - efeitos de tela (TV velha, glitch) como pos-processamento do canvas.
 *
 * Uso: ShaderFx_Set liga um efeito (vale para o frame seguinte). Game.c passa o canvas inteiro pelo shader.
 * intensidade 0..1 (0 = desligado). velocidade 0..2 (0 = estatico; 1 = normal).
 */
#pragma once
#ifndef RENDERING_SHADER_FX_H
#define RENDERING_SHADER_FX_H

#include "raylib.h"

typedef enum {
    SHADERFX_NONE = 0,
    SHADERFX_TV,
    SHADERFX_GLITCH,
    SHADERFX_BLACKNESS,
    SHADERFX_GRAYSCALE,
    SHADERFX_BLOOM,
    SHADERFX_SPACE,   /* nebulosa + estrelas animadas */
    SHADERFX_PULSE,   /* pulso de batida: zoom + brilho + borda, no ritmo */
    SHADERFX_DISTORTION, /* ondas de calor + curvatura de lente */
    SHADERFX_CHROMATIC,  /* aberracao cromatica: separa R e B do centro */
    SHADERFX_DISSOLVE,   /* dissolver: pixels somem por ruido, com borda incandescente */
    SHADERFX_DEPTH       /* ilusao de profundidade: paralaxe por luminancia + nevoa no fundo */
} ShaderFxKind;

/* Pilha de ate 3 efeitos, aplicados em ordem de entrada (encadeados em passes).
 * Respeita gameOptions.shaders: com a opcao off, nada entra na pilha. */
void ShaderFx_Set(ShaderFxKind kind, float intensity, float speed);  /* limpa a pilha e liga so este */
void ShaderFx_Push(ShaderFxKind kind, float intensity, float speed); /* empilha; tipo ja ligado: so atualiza; cheia: sai o mais antigo */
void ShaderFx_Remove(ShaderFxKind kind);                             /* tira um efeito da pilha */
void ShaderFx_Reset(void);                                           /* esvazia a pilha */
int  ShaderFx_IsActive(void);                                        /* 1 = ha efeito ligado e o shader carregou */
void ShaderFx_Present(Texture2D src, Rectangle dst, float w, float h); /* desenha o canvas em dst com a pilha */
void ShaderFx_Unload(void);                                          /* antes de fechar a janela */

#endif /* RENDERING_SHADER_FX_H */
