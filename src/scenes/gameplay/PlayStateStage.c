/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStage.c - stage do mod (Psych .json + .lua) para o PlayState.
 * Usa Stage.c apenas como parser; aqui ficam texturas, desenho e liberacao.
 */

#include "scenes/gameplay/PlayStateStage.h"
#include "logic/Stage.h"
#include "core/ModPath.h"
#include "Log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


// Stage padrao (usado quando o mod nao tem stage .json/.lua).
static void LoadDefaultStage(PlayStateStage* stage, Girlfriend* gf, Character* dad, Character* bf) {
        RayGraphicObject* funkyStageObjects = malloc(sizeof(RayGraphicObject) * 3);

        RayGraphicObject* bg = funkyStageObjects;
        RayGraphicObject* front = funkyStageObjects + 1;
        RayGraphicObject* curtains = funkyStageObjects + 2;

        Render_DefaultRGT(bg);
        bg->objType = RGT_IMAGE;
        bg->position = (Vector2) {-600, -200};
        bg->scrollFactor = (Vector2) {0.9f, 0.9f};
        bg->image = Render_LoadTexture("assets/images/stage/stageback.png");

        Render_DefaultRGT(front);
        front->objType = RGT_IMAGE;
        front->position = (Vector2) {-650, 600};
        front->scrollFactor = (Vector2) {0.9f, 0.9f};
        front->scaleX = front->scaleY = 1.1f;
        front->image = Render_LoadTexture("assets/images/stage/stagefront.png");

        Render_DefaultRGT(curtains);
        curtains->objType = RGT_IMAGE;
        curtains->position = (Vector2) {-350, -300};
        curtains->scrollFactor = (Vector2) {1.3f, 1.3f};
        curtains->scaleX = curtains->scaleY = 0.9f;
        curtains->image = Render_LoadTexture("assets/images/stage/stagecurtains.png");

        gf->object.scrollFactor = (Vector2) {0.9f, 0.9f};
        gf->object.position = (Vector2) {400, 100};
        dad->object.position = (Vector2) {200, 150};
        bf->object.position = (Vector2) {820, 500};

        stage->objects = funkyStageObjects;
        stage->objectCount = 3;
        stage->backgroundObjects = 2;
        stage->defaultCameraZoom = 0.9f;
}

// Raiz do mod (assets/mods/<mod> ou assets/mods direto).
static void ModRoot(const char* songDir, char out[256]) {
    (void)ModPath_Root(songDir, out, 256);
}

// <mod>/images/<nome>.<ext>, depois assets/images/<nome>.<ext>.
static char FindImage(const char* root, const char* name, const char* ext, char out[512]) {
    if (root[0] != '\0') {
        snprintf(out, 512, "%s/images/%s.%s", root, name, ext);
        if (FileExists(out)) return 1;
    }
    snprintf(out, 512, "assets/images/%s.%s", name, ext);
    return FileExists(out);
}

// Fundo preso a camera (scroll 0,0): cobre a tela inteira, centralizado.
// Devolve a escala (>= escala do Lua) e a posicao no sistema do engine.
static float CoverScreen(float baseW, float baseH, float luaScale, float zoom, Vector2* pos) {
    float s = luaScale;
    if (baseW > 1.0f && baseH > 1.0f && zoom > 0.01f) {
        const float needW = (1280.0f / zoom) * 1.1f / baseW;
        const float needH = (720.0f / zoom) * 1.1f / baseH;
        if (needW > s) s = needW;
        if (needH > s) s = needH;
    }
    pos->x = -baseW * s * 0.5f;
    pos->y = -baseH * s * 0.5f;
    return s;
}

// Camera_* do stage JSON (so valem durante o PlayStateStage_Load).
static const float* cameraOffsetBf;
static const float* cameraOffsetDad;

// Foco da camera como na Psych (moveCamera), calculado uma vez depois das posicoes finais.
//   dad: ponto medio + (150, -100) + camera_position + camera_opponent
//   bf:  ponto medio + (-100, -100) + (-camera_position.x, camera_position.y)
//        + (camera_boyfriend.x, camera_boyfriend.y)
// Personagem sem JSON (bf/dad padrao do engine) mantem o foco proprio, so soma o offset do stage.
static void FinalizeCamera(Character* c, char isDad, const float* stageOffset) {
    const float sx = stageOffset != NULL ? stageOffset[0] : 0.0f;
    const float sy = stageOffset != NULL ? stageOffset[1] : 0.0f;

    if (!c->psychCamera) {
        c->cameraOffset.x += sx;
        c->cameraOffset.y += sy;
        return;
    }

    // Psych: width/height = frameWidth/Height do atlas * escala. Sem XML, cai no frame idle * escala.
    // A posicao ja traz psychShift (escala cresce do centro); o ponto medio usa a posicao sem ele.
    const Vector2 size = c->psychBox.x > 0.0f ? c->psychBox : AnimatedObject_Sizes(&c->object);
    const float halfW = fabsf(size.x) * 0.5f - c->psychShift.x;
    const float halfH = fabsf(size.y) * 0.5f - c->psychShift.y;
    const Vector2 raw = c->cameraOffset;

    if (isDad)
        c->cameraOffset = (Vector2) {halfW + 150.0f + raw.x + sx, halfH - 100.0f + raw.y + sy};
    else
        c->cameraOffset = (Vector2) {halfW - 100.0f - raw.x + sx, halfH - 100.0f + raw.y + sy};
    c->psychCamera = false;   // ja convertido
}

// Carrega o stage do mod. Retorna 0 se nao existe (usa o padrao).
static char LoadModStage(PlayStateStage* stage, const char* name, const char* songDir,
                         Girlfriend* gf, Character* dad, Character* bf) {
    static StageSprite sprites[STAGE_MAX_SPRITES];
    char luaPath[300], jsonPath[300];

    char hasLua = Stage_FindLua(name, songDir, luaPath);
    char hasJson = Stage_FindJson(name, songDir, jsonPath);
    // Stage "stage" (ou vazio) = padrao do engine. Qualquer outro nome sem
    // arquivos vira um "void" vazio, como na Psych (posicoes e zoom padrao).
    if (!hasLua && !hasJson && (name[0] == '\0' || strcmp(name, "stage") == 0)) return 0;

    if (!hasLua && !hasJson)
        Log_WarnF("Stage '%s' nao encontrado: usando stage vazio", name);
    else if (!hasJson)
        Log_WarnF("Stage '%s': sem .json (posicoes e zoom padrao)", name);

    StageMeta meta;
    if (!Stage_LoadJson(hasJson ? jsonPath : "", &meta) && hasJson)
        Log_WarnF("Stage '%s': .json invalido", name);

    int count = 0;
    if (hasLua) {
        char* text = LoadFileText(luaPath);
        if (text != NULL) {
            count = Stage_ParseLua(text, sprites, STAGE_MAX_SPRITES, NULL, NULL);
            UnloadFileText(text);
        }
    }

    char root[256];
    ModRoot(songDir, root);

    if (count > 0) {
        stage->objects = malloc(sizeof(RayGraphicObject) * count);
        stage->animObjects = calloc((size_t)count, sizeof(RayAnimatedObject));
    }

    // Passo 0 = atras dos personagens, passo 1 = na frente.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            const StageSprite* sp = sprites + i;
            if (sp->front != pass) continue;

            char png[512], xml[512];
            Texture2D tex = {0};
            char hasXml = 0;

            if (!sp->isGraphic) {
                if (!FindImage(root, sp->image, "png", png)) {
                    Log_WarnF("Stage: imagem nao encontrada: %s", sp->image);
                    continue;
                }
                tex = Render_LoadTexture(png);
                if (tex.id == 0) continue;
                hasXml = sp->isAnimated && FindImage(root, sp->image, "xml", xml);
            }

            if (hasXml) {
                RayAnimatedObject* o = stage->animObjects + stage->animCount++;
                Render_DefaultAnimated(o);
                o->animationSet = AnimationSet_LoadAnimations(xml, tex);
                o->scrollFactor = (Vector2) {sp->scrollX, sp->scrollY};
                o->scaleX = 1.0f;
                o->scaleY = 1.0f;

                int idx = sp->animPrefix[0] ? AnimationSet_FindAnimation(&o->animationSet, sp->animPrefix) : -1;
                if (idx < 0) idx = 0;

                // Psych: scroll em relacao ao canto da tela; o engine usa o centro
                // (por isso o -640/-360*(1-scroll) abaixo). Escala parte do canto superior esquerdo
                // (scaleObject do Psych usa updateHitbox).
                Vector2 base = {0.0f, 0.0f};
                if (o->animationSet.animationCount > 0) {
                    AnimatedObject_SetAnimation(o, idx);
                    base = AnimatedObject_Sizes(o);
                }

                o->scaleX = sp->scaleX;
                o->scaleY = sp->scaleY;
                o->position = (Vector2) {
                    sp->x - 640.0f * (1.0f - sp->scrollX),
                    sp->y - 360.0f * (1.0f - sp->scrollY)
                };
                if (sp->scaleCenter) {
                    o->position.x -= base.x * (sp->scaleX - 1.0f) * 0.5f;
                    o->position.y -= base.y * (sp->scaleY - 1.0f) * 0.5f;
                }
                if (sp->scrollX == 0.0f && sp->scrollY == 0.0f && pass == 0) {
                    float fit = CoverScreen(base.x, base.y, sp->scaleX, meta.defaultZoom, &o->position);
                    o->scaleX = o->scaleY = fit;
                }

                if (pass == 0) stage->animBackground++;
            } else {
                RayGraphicObject* g = stage->objects + stage->objectCount++;
                Render_DefaultRGT(g);
                const float baseW = sp->isGraphic ? sp->graphicWidth : (float)tex.width;
                const float baseH = sp->isGraphic ? sp->graphicHeight : (float)tex.height;
                // Psych (scaleObject + updateHitbox): a escala parte do canto superior esquerdo.
                g->position = (Vector2) {
                    sp->x - 640.0f * (1.0f - sp->scrollX),
                    sp->y - 360.0f * (1.0f - sp->scrollY)
                };
                g->scrollFactor = (Vector2) {sp->scrollX, sp->scrollY};
                g->scaleX = sp->scaleX;
                g->scaleY = sp->scaleY;
                if (sp->scaleCenter) {
                    // setProperty('x.scale.x'): sem updateHitbox, cresce do centro.
                    g->position.x -= baseW * (sp->scaleX - 1.0f) * 0.5f;
                    g->position.y -= baseH * (sp->scaleY - 1.0f) * 0.5f;
                }
                if (!sp->isGraphic && sp->scrollX == 0.0f && sp->scrollY == 0.0f && pass == 0) {
                    float fit = CoverScreen(baseW, baseH, sp->scaleX, meta.defaultZoom, &g->position);
                    g->scaleX = g->scaleY = fit;
                }

                if (sp->isGraphic) {
                    g->objType = RGT_RECTANGLE;
                    g->rectSize = (Vector2) {sp->graphicWidth, sp->graphicHeight};
                    g->color = (Color) {sp->colorR, sp->colorG, sp->colorB, sp->colorA};
                } else {
                    g->objType = RGT_IMAGE;
                    g->image = tex;
                }

                if (pass == 0) stage->backgroundObjects++;
            }
        }
    }

    stage->defaultCameraZoom = meta.defaultZoom;
    stage->hideGirlfriend = (char)meta.hideGirlfriend;
    stage->cameraSpeed = meta.cameraSpeed;
    cameraOffsetBf = meta.cameraBoyfriend;
    cameraOffsetDad = meta.cameraOpponent;

    gf->object.scrollFactor = (Vector2) {0.95f, 0.95f};
    // Posicao final = posicao do stage + offset proprio do personagem (como na Psych).
    gf->object.position.x += meta.girlfriend[0] - 640.0f * 0.05f;
    gf->object.position.y += meta.girlfriend[1] - 360.0f * 0.05f;
    dad->object.position.x += meta.opponent[0];
    dad->object.position.y += meta.opponent[1];
    bf->object.position.x += meta.boyfriend[0];
    bf->object.position.y += meta.boyfriend[1];
    return 1;
}

void PlayStateStage_Load(PlayStateStage* stage, const char* stageName, const char* songDir,
                         Girlfriend* gf, Character* dad, Character* bf)
{
    memset(stage, 0, sizeof(*stage));
    stage->cameraSpeed = 1.0f;
    cameraOffsetBf = NULL;
    cameraOffsetDad = NULL;
    if (!LoadModStage(stage, stageName, songDir, gf, dad, bf)) {
        free(stage->objects);
        free(stage->animObjects);
        memset(stage, 0, sizeof(*stage));
        stage->cameraSpeed = 1.0f;
        LoadDefaultStage(stage, gf, dad, bf);
    }
    FinalizeCamera(dad, 1, cameraOffsetDad);
    FinalizeCamera(bf, 0, cameraOffsetBf);
}

void PlayStateStage_DrawBack(PlayStateStage* stage, char paused)
{
    for (int i = 0; i < stage->backgroundObjects; i++)
        Render_DrawGraphicObject(stage->objects + i);

    for (int i = 0; i < stage->animBackground; i++) {
        if (!paused) AnimatedObject_UpdateFrame(stage->animObjects + i);
        Render_DrawAnimatedObject(stage->animObjects + i);
    }
}

void PlayStateStage_DrawFront(PlayStateStage* stage, char paused)
{
    for (int i = stage->backgroundObjects; i < stage->objectCount; i++)
        Render_DrawGraphicObject(stage->objects + i);

    for (int i = stage->animBackground; i < stage->animCount; i++) {
        if (!paused) AnimatedObject_UpdateFrame(stage->animObjects + i);
        Render_DrawAnimatedObject(stage->animObjects + i);
    }
}

void PlayStateStage_Unload(PlayStateStage* stage)
{
    for (int i = 0; i < stage->objectCount; i++)
        UnloadTexture(stage->objects[i].image);

    for (int i = 0; i < stage->animCount; i++)
        AnimationSet_FreeAll(&stage->animObjects[i].animationSet);

    free(stage->objects);
    free(stage->animObjects);
    memset(stage, 0, sizeof(*stage));
}
