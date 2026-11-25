/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ShaderTranslate.c - tradutor minimo de shaders OpenFL/Flixel para GLSL 3.30.
 */
#include "rendering/shaders/ShaderTranslate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cabecalho: define as entradas e os nomes antigos usados pelos shaders do Flixel. */
static const char kHeader[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 openfl_TextureSize;\n"
    "#define bitmap texture0\n"
    "#define openfl_TextureCoordv fragTexCoord\n"
    "#define flixel_texture2D(b, p) texture(b, p)\n"
    "#define texture2D(b, p) texture(b, p)\n"
    "#define gl_FragColor finalColor\n";

/* Shadertoy usa mainImage(out vec4, in vec2); envolvemos com main(). */
static const char kMainImageTail[] =
    "void main() { mainImage(finalColor, fragTexCoord * openfl_TextureSize); }\n";

/* Linha comeca com a diretiva (ignora espacos antes do '#'). */
static int StartsDirective(const char* line, const char* name) {
    while (*line == ' ' || *line == '\t') line++;
    return strncmp(line, name, strlen(name)) == 0;
}

char* ShaderTranslate_Flixel(const char* src) {
    if (src == NULL) return NULL;

    const size_t srcLen = strlen(src);
    const int needWrap = strstr(src, "void main(") == NULL && strstr(src, "mainImage(") != NULL;
    const size_t cap = (sizeof kHeader - 1) + srcLen + 1 + (sizeof kMainImageTail - 1) + 1;

    char* out = malloc(cap);
    if (out == NULL) return NULL;

    size_t len = sizeof kHeader - 1;
    memcpy(out, kHeader, len);

    /* Copia linha a linha, pulando #version e #pragma (o antigo "#pragma header"). */
    const char* p = src;
    while (*p != '\0') {
        const char* eol = strchr(p, '\n');
        const size_t lineLen = eol != NULL ? (size_t)(eol - p) + 1 : strlen(p);
        if (!StartsDirective(p, "#version") && !StartsDirective(p, "#pragma")) {
            memcpy(out + len, p, lineLen);
            len += lineLen;
        }
        p += lineLen;
    }

    if (needWrap) {
        out[len++] = '\n';
        memcpy(out + len, kMainImageTail, sizeof kMainImageTail - 1);
        len += sizeof kMainImageTail - 1;
    }
    out[len] = '\0';
    return out;
}

char* ShaderTranslate_FlixelFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    const long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(f);
        return NULL;
    }
    const size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    text[got] = '\0';

    char* out = ShaderTranslate_Flixel(text);
    free(text);
    return out;
}

FlixelShader FlixelShader_LoadFile(const char* path) {
    FlixelShader s = {0};
    char* glsl = ShaderTranslate_FlixelFile(path);
    if (glsl == NULL) return s;

    s.sh = LoadShaderFromMemory(NULL, glsl);
    free(glsl);
    s.ok = (char)(s.sh.id != 0);
    if (s.ok) s.locTexSize = GetShaderLocation(s.sh, "openfl_TextureSize");
    return s;
}

void FlixelShader_BeginDraw(FlixelShader* s, float texW, float texH) {
    if (s == NULL || !s->ok) return;
    const float size[2] = {texW, texH};
    if (s->locTexSize >= 0) SetShaderValue(s->sh, s->locTexSize, size, SHADER_UNIFORM_VEC2);
    BeginShaderMode(s->sh);
}

void FlixelShader_EndDraw(FlixelShader* s) {
    if (s == NULL || !s->ok) return;
    EndShaderMode();
}

void FlixelShader_Unload(FlixelShader* s) {
    if (s == NULL || !s->ok) return;
    UnloadShader(s->sh);
    s->ok = 0;
}
