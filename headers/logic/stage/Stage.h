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
    uint8_t scaleCenter; /* 1 = escala via setProperty (Psych cresce do centro, sem updateHitbox) */

    int front;           /* 0 = atras, 1 = frente (addLuaSprite/setObjectOrder) */

    /* makeAnimatedLuaSprite + addAnimationByPrefix (primeira animacao).
     * animPrefix vazio = usar a primeira animacao do atlas .xml.
     */
    uint8_t isAnimated;
    uint8_t animLoop;
    float animFps;
    char animPrefix[64];

    /* setProperty/updateHitbox/screenCenter/setGraphicSize (0 = padrao). */
    uint8_t hidden;      /* visible = false */
    uint8_t flipX;
    uint8_t center;      /* screenCenter: 1 = x, 2 = y */
    float sizeW, sizeH;  /* setGraphicSize; sizeH 0 = mantem proporcao */
} StageSprite;

/* ---- Partes do script usadas so no PlayState (StageScript) ---- */

#define STAGE_MAX_ANIM_DEFS 64
#define STAGE_MAX_ACTIONS   64
#define STAGE_COND_MAX      4

/* addAnimationByPrefix / addAnimationByIndices(+Loop) / addOffset. */
typedef struct {
    uint8_t sprite;      /* indice em StageSprite[] */
    uint8_t loop;
    float fps;
    float offsetX, offsetY;
    char name[32];
    char prefix[64];
    char indices[128];   /* "0,1,2"; vazio = todos os frames do prefixo */
} StageAnimDef;

typedef enum { STAGE_VAR_BEAT, STAGE_VAR_STEP, STAGE_VAR_SECTION, STAGE_VAR_COUNTER } StageVar;
typedef enum { STAGE_OP_EQ, STAGE_OP_NE, STAGE_OP_LT, STAGE_OP_LE, STAGE_OP_GT, STAGE_OP_GE } StageOp;

/* if curBeat % mod <op> value (mod 0 = sem %). */
typedef struct {
    uint8_t var, op;
    int16_t mod;
    int32_t value;
} StageCond;

typedef enum {
    STAGE_TRIGGER_CREATE,     /* onCreate/onCreatePost/topo: animacao inicial */
    STAGE_TRIGGER_COUNTDOWN,  /* onCountdownTick(counter) */
    STAGE_TRIGGER_BEAT,       /* onBeatHit */
    STAGE_TRIGGER_STEP,       /* onStepHit */
    STAGE_TRIGGER_SECTION     /* onSectionHit */
} StageTrigger;

/* playAnim/objectPlayAnimation dentro de uma callback. */
typedef struct {
    uint8_t sprite, trigger, forced, condCount;
    StageCond cond[STAGE_COND_MAX];
    char anim[32];
} StageAnimAction;

typedef struct {
    StageAnimDef anims[STAGE_MAX_ANIM_DEFS];
    StageAnimAction actions[STAGE_MAX_ACTIONS];
    int animCount, actionCount;
    float camZoom;           /* setProperty('defaultCamZoom') no create; 0 = do .json */
    uint8_t hideGirlfriend;  /* setProperty('gf.visible', false) */
    int ignoredGlitch, ignored;
} StageScript;

/* 1 se todas as condicoes valem para os contadores dados. */
int Stage_CondsMatch(const StageCond *cond, int count, int beat, int step, int section, int counter);

/* Dados do <nome>.json do stage (formato Psych 0.6/0.7).
 * Os valores iniciais sao os padroes da Psych.
 */
typedef struct {
    float defaultZoom;
    uint8_t isPixelStage;
    uint8_t hideGirlfriend;   /* hide_girlfriend: igual gfVersion "nogf" */
    float boyfriend[2];
    float girlfriend[2];
    float opponent[2];
    float cameraBoyfriend[2];   /* camera_boyfriend (padrao 0,0) */
    float cameraOpponent[2];    /* camera_opponent (padrao 0,0) */
    float cameraSpeed;          /* camera_speed (padrao 1) */
} StageMeta;

/* Procura <nome>.json nos mesmos lugares do .lua. Retorna 1 se achou. */
char Stage_FindJson(const char *stageName, const char *songDir, char out[300]);

/* Igual ao Stage_FindJson, mas aceita o nome "stage" (usado pelo stage padrao do engine,
 * que le so o json de posicoes). */
char Stage_FindJsonAllowStage(const char *stageName, const char *songDir, char out[300]);

/* Procura <nome>.lua em <mod>/stages/, <mod>/data/stages/,
 * assets/stages/, assets/data/stages/ e songDir/.
 * Retorna 1 quando encontrado; escreve o caminho em out[300].
 */
char Stage_FindLua(const char *stageName, const char *songDir, char out[300]);

#endif /* STAGE_H */
