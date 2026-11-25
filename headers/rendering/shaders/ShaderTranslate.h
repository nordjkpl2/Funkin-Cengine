/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ShaderTranslate.h - tradutor minimo de shaders estilo OpenFL/Flixel (#pragma header, flixel_texture2D)
 * para GLSL 3.30 (raylib/OpenGL 3.3).
 *
 * Nao e um compilador: troca so o cabecalho, o nome de saida (gl_FragColor) e envolve mainImage (Shadertoy).
 * Uniforms extras (ex.: openfl_TextureSize) devem ser setados pelo chamador.
 */
#pragma once
#ifndef RENDERING_SHADER_TRANSLATE_H
#define RENDERING_SHADER_TRANSLATE_H

#include "raylib.h"

/* Retorna string nova (free()) com o GLSL 3.30 pronto para LoadShaderFromMemory, ou NULL. */
char* ShaderTranslate_Flixel(const char* src);

/* Le o arquivo .frag e traduz. Retorna string nova (free()) ou NULL. */
char* ShaderTranslate_FlixelFile(const char* path);

/* Shader traduzido + localizacao do uniform openfl_TextureSize (setado no BeginDraw). */
typedef struct {
    Shader sh;
    int locTexSize;
    char ok;
} FlixelShader;

FlixelShader FlixelShader_LoadFile(const char* path); /* ok = 0 se falhar */
void FlixelShader_BeginDraw(FlixelShader* s, float texW, float texH); /* seta uniform e liga o shader */
void FlixelShader_EndDraw(FlixelShader* s);
void FlixelShader_Unload(FlixelShader* s);

#endif /* RENDERING_SHADER_TRANSLATE_H */
