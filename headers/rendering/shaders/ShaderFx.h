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

typedef enum {
    SHADERFX_NONE = 0,
    SHADERFX_TV,
    SHADERFX_GLITCH,
    SHADERFX_BLACKNESS,
    SHADERFX_GRAYSCALE,
    SHADERFX_BLOOM,
    SHADERFX_SPACE,   /* nebulosa + estrelas animadas */
    SHADERFX_PULSE    /* pulso de batida: zoom + brilho + borda, no ritmo */
} ShaderFxKind;

/* Respeita gameOptions.shaders: com a opcao off, ShaderFx_Set nao liga nada. */
void ShaderFx_Set(ShaderFxKind kind, float intensity, float speed);
void ShaderFx_Reset(void);
int  ShaderFx_IsActive(void);            /* 1 = ha efeito ligado e o shader carregou */
int  ShaderFx_BeginDraw(float w, float h); /* 1 = shader ativo; chame ShaderFx_EndDraw depois */
void ShaderFx_EndDraw(void);
void ShaderFx_Unload(void);              /* antes de fechar a janela */

#endif /* RENDERING_SHADER_FX_H */
