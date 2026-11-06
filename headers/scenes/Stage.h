#pragma once
#ifndef STAGE_H
#define STAGE_H

#include <stdint.h>

/* Parser subset de stage .lua da Psych, sem LuaJIT/VM (somente no load).
 * Nao executa Lua: apenas extrai comandos de criacao e propriedades de sprites.
 */

#define STAGE_MAX_SPRITES 32

typedef struct {
    char image[128];

    float x, y;
    float scrollX, scrollY;
    float scaleX, scaleY;

    /* makeGraphic(tag, width, height, color). Dimensoes sao independentes
     * de scaleX/scaleY: o tamanho desenhado e graphicWidth*scaleX por
     * graphicHeight*scaleY.
     */
    float graphicWidth, graphicHeight;
    uint8_t colorR, colorG, colorB, colorA;
    uint8_t isGraphic;

    int front;           /* 0 = atras, 1 = frente (addLuaSprite/setObjectOrder) */

    /* makeAnimatedLuaSprite + addAnimationByPrefix (primeira animacao).
     * animPrefix vazio = usar a primeira animacao do atlas .xml.
     */
    uint8_t isAnimated;
    uint8_t animLoop;
    float animFps;
    char animPrefix[64];
} StageSprite;

/* Dados do <nome>.json do stage (formato Psych 0.6/0.7).
 * Os valores iniciais sao os padroes da Psych.
 */
typedef struct {
    float defaultZoom;
    uint8_t isPixelStage;
    float boyfriend[2];
    float girlfriend[2];
    float opponent[2];
} StageMeta;

/* Procura <nome>.json nos mesmos lugares do .lua. Retorna 1 se achou. */
char Stage_FindJson(const char *stageName, const char *songDir, char out[300]);

/* Le o JSON. Sempre preenche *out (padroes quando falta campo).
 * Retorna 1 se o arquivo foi lido, 0 se nao existe ou e invalido.
 */
char Stage_LoadJson(const char *path, StageMeta *out);

/* Procura <nome>.lua em <mod>/stages/, <mod>/data/stages/,
 * assets/stages/, assets/data/stages/ e songDir/.
 * Retorna 1 quando encontrado; escreve o caminho em out[300].
 */
char Stage_FindLua(const char *stageName, const char *songDir, char out[300]);

/* Faz parse do subset de Lua suportado para out[capacity]. Retorna quantidade.
 * ignoredGlitch/ignoredAnim podem ser NULL.
 */
int Stage_ParseLua(const char *luaText, StageSprite *out, int capacity,
                   int *ignoredGlitch, int *ignoredAnim);

#endif /* STAGE_H */
