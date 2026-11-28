/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateStage.c - stage do mod (Psych .json + .lua) para o PlayState.
 * Usa Stage.c apenas como parser; aqui ficam texturas, desenho e liberacao.
 */

#include "scenes/gameplay/stage/PlayStateStage.h"
#include "logic/stage/Stage.h"
#include "logic/stage/StageLuaParse.h"
#include "logic/stage/StageJson.h"
#include "core/ModPath.h"
#include "core/Log.h"
#include "rendering/cpu/ImagePrefetch.h"
#include "rendering/shaders/ShaderFx.h"
#include "logic/animations/AnimSetParseUtil.h"
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

// Stage padrao ("stage") com <stage>.json. Igual a Psych: boyfriend/opponent/girlfriend sao a
// posicao BASE absoluta do stage; a posicao do personagem (position do JSON + psychShift, guardada
// em offX) e somada por cima (startCharacterPos). Sem o json, vale o padrao fixo do LoadDefaultStage.
static float defaultCamBf[2], defaultCamDad[2];
static char ApplyDefaultStageJson(const char* songDir, Girlfriend* gf, Character* dad, Character* bf,
                                  Vector2 offGf, Vector2 offDad, Vector2 offBf) {
    char jsonPath[300];
    if (!Stage_FindJsonAllowStage("stage", songDir, jsonPath)) return 0;
    StageMeta meta;
    if (!Stage_LoadJson(jsonPath, &meta)) return 0;
    gf->object.position = (Vector2) {meta.girlfriend[0] - 640.0f * 0.05f + offGf.x,
                                     meta.girlfriend[1] - 360.0f * 0.05f + offGf.y};
    dad->object.position = (Vector2) {meta.opponent[0] + offDad.x, meta.opponent[1] + offDad.y};
    bf->object.position = (Vector2) {meta.boyfriend[0] + offBf.x, meta.boyfriend[1] + offBf.y};
    defaultCamBf[0] = meta.cameraBoyfriend[0]; defaultCamBf[1] = meta.cameraBoyfriend[1];
    defaultCamDad[0] = meta.cameraOpponent[0]; defaultCamDad[1] = meta.cameraOpponent[1];
    return 1;
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

static StageSprite sprites[STAGE_MAX_SPRITES]; // compartilhado por Prefetch e LoadModStage
static StageScript script;                     // animacoes e playAnim do .lua (so no load)
static PlayStateStage* activeStage;            // stage carregado: recebe as batidas

// Inicia a decodificacao das imagens do stage em fundo. Mesma busca do Load; erros ficam para ele.
void PlayStateStage_Prefetch(const char* name, const char* songDir) {
    char luaPath[300], jsonPath[300];
    char hasLua = Stage_FindLua(name, songDir, luaPath);
    char hasJson = Stage_FindJson(name, songDir, jsonPath);

    if (!hasLua) {
        if (!hasJson && (name[0] == '\0' || strcmp(name, "stage") == 0)) {
            ImagePrefetch_Start("assets/images/stage/stageback.png");
            ImagePrefetch_Start("assets/images/stage/stagefront.png");
            ImagePrefetch_Start("assets/images/stage/stagecurtains.png");
        }
        return;
    }

    char* text = LoadFileText(luaPath);
    if (text == NULL) return;
    int count = Stage_ParseLua(text, sprites, STAGE_MAX_SPRITES, NULL, NULL);
    UnloadFileText(text);

    char root[256];
    ModRoot(songDir, root);
    for (int i = 0; i < count; i++) {
        char png[512];
        if (!sprites[i].isGraphic && FindImage(root, sprites[i].image, "png", png))
            ImagePrefetch_Start(png);
    }
}

char PlayStateStage_HidesGirlfriend(const char* name, const char* songDir) {
    char jsonPath[300];
    StageMeta meta;
    if (!Stage_FindJson(name, songDir, jsonPath)) return 0;
    return Stage_LoadJson(jsonPath, &meta) && meta.hideGirlfriend;
}

// Psych addAnimationByPrefix: nome igual, depois nome que comeca com o prefixo
// (o atlas junta os frames pelo nome sem os digitos e corta em 31 letras).
static int FindAtlasPrefix(const RayAnimationHandler* atlas, const char* prefix) {
    const int exact = AnimationSet_FindAnimation(atlas, prefix);
    if (exact >= 0) return exact;
    const size_t length = strlen(prefix);
    const size_t maxName = sizeof(atlas->animations[0].name) - 1;
    for (int i = 0; i < atlas->animationCount; i++) {
        const char* name = atlas->animations[i].name;
        if (strncmp(name, prefix, length) == 0) return i;
        if (length >= maxName && strncmp(name, prefix, maxName) == 0) return i;
    }
    return -1;
}

// Frames de "0,1,2" (addAnimationByIndices); vazio = todos. dst NULL so conta.
static int PickFrames(const char* indices, const RayAnimation* src, Frame* dst) {
    if (indices[0] == '\0') {
        if (dst) memcpy(dst, src->frames, sizeof(Frame) * (size_t)src->frameCount);
        return src->frameCount;
    }
    int count = 0;
    for (const char* p = indices; *p != '\0';) {
        char* end = NULL;
        const long index = strtol(p, &end, 10);
        if (end == p) { p++; continue; }
        p = end;
        if (index < 0 || index >= src->frameCount) continue;
        if (dst) dst[count] = src->frames[index];
        count++;
    }
    return count;
}

// Troca as animacoes do atlas pelas do .lua (nome, fps, loop, indices, offset). 0 = sem nenhuma.
static char BuildLuaAnimations(RayAnimationHandler* set, int sprite, const char* image) {
    int defs = 0, total = 0;
    for (int i = 0; i < script.animCount; i++) {
        const StageAnimDef* def = script.anims + i;
        if (def->sprite != sprite) continue;
        const int src = FindAtlasPrefix(set, def->prefix);
        if (src < 0) {
            Log_WarnF("Stage: '%s' sem animacao '%s' no .xml", image, def->prefix);
            continue;
        }
        total += PickFrames(def->indices, set->animations + src, NULL);
        defs++;
    }
    if (defs == 0 || total == 0) return 0;

    RayAnimation* anims = calloc((size_t)defs, sizeof(RayAnimation));
    Frame* arena = malloc(sizeof(Frame) * (size_t)total);
    if (anims == NULL || arena == NULL) {
        free(anims);
        free(arena);
        return 0;
    }

    int count = 0;
    Frame* cursor = arena;
    for (int i = 0; i < script.animCount; i++) {
        const StageAnimDef* def = script.anims + i;
        if (def->sprite != sprite) continue;
        const int src = FindAtlasPrefix(set, def->prefix);
        if (src < 0) continue;
        const int frames = PickFrames(def->indices, set->animations + src, cursor);
        if (frames <= 0) continue;

        RayAnimation* a = anims + count++;
        const float fps = def->fps > 0.0f ? def->fps : 24.0f;
        snprintf(a->name, sizeof(a->name), "%s", def->name);
        a->frames = cursor;
        a->frameCount = frames;
        a->fps = (int)lroundf(fps);
        a->frameTime = 1.0f / fps;
        a->looped = (char)def->loop;
        a->animationOffset = (Vector2) {def->offsetX, def->offsetY};
        cursor += frames;
    }

    AnimationSet_FreeArrays(set); // textura continua no set
    set->animations = anims;
    set->frameArena = arena;
    set->animationCount = count;
    return 1;
}

// Escala final: setGraphicSize vence scaleObject (FlxSprite.setGraphicSize).
static void LuaScale(const StageSprite* sp, Vector2 base, float* sx, float* sy) {
    *sx = sp->scaleX;
    *sy = sp->scaleY;
    if ((sp->sizeW > 0.0f || sp->sizeH > 0.0f) && base.x > 0.0f && base.y > 0.0f) {
        *sx = sp->sizeW / base.x;
        *sy = sp->sizeH / base.y;
        if (sp->sizeW <= 0.0f) *sx = *sy;
        else if (sp->sizeH <= 0.0f) *sy = *sx;
    }
}

// Posicao Psych (canto, com screenCenter) -> engine. Psych: scroll em relacao ao canto da tela;
// o engine usa o centro (por isso o -640/-360*(1-scroll)). Sem updateHitbox a escala cresce do centro.
static Vector2 LuaPosition(const StageSprite* sp, Vector2 base, float sx, float sy) {
    float x = sp->x, y = sp->y;
    if (sp->center & 1) x = (1280.0f - base.x * (sp->scaleCenter ? 1.0f : fabsf(sx))) * 0.5f;
    if (sp->center & 2) y = (720.0f - base.y * (sp->scaleCenter ? 1.0f : fabsf(sy))) * 0.5f;
    Vector2 pos = {x - 640.0f * (1.0f - sp->scrollX), y - 360.0f * (1.0f - sp->scrollY)};
    if (sp->scaleCenter) {
        pos.x -= base.x * (sx - 1.0f) * 0.5f;
        pos.y -= base.y * (sy - 1.0f) * 0.5f;
    }
    return pos;
}

// playAnim do .lua: anima inicial (create) ou guarda para as batidas.
static void LoadLuaActions(PlayStateStage* stage, const short* animOf) {
    int total = 0;
    for (int i = 0; i < script.actionCount; i++)
        if (script.actions[i].trigger != STAGE_TRIGGER_CREATE && animOf[script.actions[i].sprite] >= 0) total++;
    if (total > 0) stage->actions = malloc(sizeof(PlayStateStageAction) * (size_t)total);

    for (int i = 0; i < script.actionCount; i++) {
        const StageAnimAction* a = script.actions + i;
        const short object = animOf[a->sprite];
        if (object < 0) continue;
        RayAnimatedObject* o = stage->animObjects + object;
        const int anim = AnimationSet_FindAnimation(&o->animationSet, a->anim);
        if (anim < 0) continue;

        if (a->trigger == STAGE_TRIGGER_CREATE) {
            AnimatedObject_SetAnimation(o, anim);
            continue;
        }
        if (stage->actions == NULL) continue;
        PlayStateStageAction* out = stage->actions + stage->actionCount++;
        memcpy(out->cond, a->cond, sizeof(out->cond));
        out->condCount = a->condCount;
        out->trigger = a->trigger;
        out->forced = a->forced;
        out->object = object;
        out->anim = (short)anim;
    }
}

// Carrega o stage do mod. Retorna 0 se nao existe (usa o padrao).
static char LoadModStage(PlayStateStage* stage, const char* name, const char* songDir,
                         Girlfriend* gf, Character* dad, Character* bf) {
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
    memset(&script, 0, sizeof(script));
    if (hasLua) {
        char* text = LoadFileText(luaPath);
        if (text != NULL) {
            count = Stage_ParseLuaScript(text, sprites, STAGE_MAX_SPRITES, &script);
            UnloadFileText(text);
        }
    }
    if (script.camZoom > 0.0f) meta.defaultZoom = script.camZoom;

    char root[256];
    ModRoot(songDir, root);

    if (count > 0) {
        stage->objects = malloc(sizeof(RayGraphicObject) * count);
        stage->animObjects = calloc((size_t)count, sizeof(RayAnimatedObject));
        stage->items = malloc(sizeof(PlayStateStageItem) * count);
    }

    short animOf[STAGE_MAX_SPRITES];  // sprite do .lua -> animObjects (-1 = nao animado)
    for (int i = 0; i < STAGE_MAX_SPRITES; i++) animOf[i] = -1;

    // Passo 0 = atras dos personagens, passo 1 = na frente. Dentro do passo vale a ordem do addLuaSprite.
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < count; i++) {
            const StageSprite* sp = sprites + i;
            if (sp->front != pass || sp->hidden) continue;

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
            const Color tint = {sp->colorR, sp->colorG, sp->colorB, sp->colorA};

            if (hasXml) {
                const short index = (short)stage->animCount++;
                RayAnimatedObject* o = stage->animObjects + index;
                Render_DefaultAnimated(o);
                o->animationSet = AnimationSet_LoadAnimations(xml, tex);
                o->scrollFactor = (Vector2) {sp->scrollX, sp->scrollY};

                // Sem addAnimation no .lua: as animacoes do atlas em loop.
                if (!BuildLuaAnimations(&o->animationSet, i, sp->image)) {
                    for (int a = 0; a < o->animationSet.animationCount; a++)
                        o->animationSet.animations[a].looped = 1;
                }

                // A Psych toca a primeira animacao adicionada.
                Vector2 base = {0.0f, 0.0f};
                if (o->animationSet.animationCount > 0) {
                    AnimatedObject_SetAnimation(o, 0);
                    base = AnimatedObject_Sizes(o);
                }

                float sx, sy;
                LuaScale(sp, base, &sx, &sy);
                o->scaleX = sx;
                o->scaleY = sy;
                o->position = LuaPosition(sp, base, sx, sy);
                if (sp->scrollX == 0.0f && sp->scrollY == 0.0f && pass == 0) {
                    float fit = CoverScreen(base.x, base.y, sx, meta.defaultZoom, &o->position);
                    o->scaleX = o->scaleY = fit;
                }
                if (sp->flipX) o->scaleX = -o->scaleX;
                o->color = tint;

                animOf[i] = index;
                stage->items[stage->itemCount++] = (PlayStateStageItem) {index, 1};
                if (pass == 0) stage->animBackground++;
            } else {
                const short index = (short)stage->objectCount++;
                RayGraphicObject* g = stage->objects + index;
                Render_DefaultRGT(g);
                const Vector2 base = {sp->isGraphic ? sp->graphicWidth : (float)tex.width,
                                      sp->isGraphic ? sp->graphicHeight : (float)tex.height};
                float sx, sy;
                LuaScale(sp, base, &sx, &sy);
                g->position = LuaPosition(sp, base, sx, sy);
                g->scrollFactor = (Vector2) {sp->scrollX, sp->scrollY};
                g->scaleX = sx;
                g->scaleY = sy;
                if (!sp->isGraphic && sp->scrollX == 0.0f && sp->scrollY == 0.0f && pass == 0) {
                    float fit = CoverScreen(base.x, base.y, sx, meta.defaultZoom, &g->position);
                    g->scaleX = g->scaleY = fit;
                }
                g->color = tint;

                if (sp->isGraphic) {
                    g->objType = RGT_RECTANGLE;
                    g->rectSize = (Vector2) {sp->graphicWidth, sp->graphicHeight};
                } else {
                    g->objType = RGT_IMAGE;
                    g->image = tex;
                }

                stage->items[stage->itemCount++] = (PlayStateStageItem) {index, 0};
                if (pass == 0) stage->backgroundObjects++;
            }
        }
        if (pass == 0) stage->itemBack = stage->itemCount;
    }
    LoadLuaActions(stage, animOf);
    if (script.hideGirlfriend) stage->hideGirlfriend = 1;

    stage->defaultCameraZoom = meta.defaultZoom;
    stage->hideGirlfriend |= (char)meta.hideGirlfriend;
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
    // Teste de render: sem stage nenhum (so as notas). Posicoes e zoom do stage padrao, nada carregado.
    if (strcmp(stageName, "__not_have_a_stage") == 0) {
        stage->defaultCameraZoom = 0.9f;
        gf->object.position = (Vector2) {400, 100};
        dad->object.position = (Vector2) {200, 150};
        bf->object.position = (Vector2) {820, 500};
        FinalizeCamera(dad, 1, NULL);
        FinalizeCamera(bf, 0, NULL);
        return;
    }
    if (!LoadModStage(stage, stageName, songDir, gf, dad, bf)) {
        free(stage->objects);
        free(stage->animObjects);
        free(stage->items);
        free(stage->actions);
        memset(stage, 0, sizeof(*stage));
        stage->cameraSpeed = 1.0f;
        // offset de cada personagem (position do JSON + psychShift) antes do padrao sobrescrever
        const Vector2 offGf = gf->object.position, offDad = dad->object.position, offBf = bf->object.position;
        LoadDefaultStage(stage, gf, dad, bf);
        if (ApplyDefaultStageJson(songDir, gf, dad, bf, offGf, offDad, offBf)) {
            cameraOffsetBf = defaultCamBf;
            cameraOffsetDad = defaultCamDad;
        }
    }
    FinalizeCamera(dad, 1, cameraOffsetDad);
    FinalizeCamera(bf, 0, cameraOffsetBf);
    activeStage = stage;
}

void PlayStateStage_Trigger(int trigger, int beat, int step, int section, int counter)
{
    const PlayStateStage* stage = activeStage;
    if (stage == NULL) return;

    for (int i = 0; i < stage->actionCount; i++) {
        const PlayStateStageAction* a = stage->actions + i;
        if (a->trigger != trigger || !Stage_CondsMatch(a->cond, a->condCount, beat, step, section, counter))
            continue;
        RayAnimatedObject* o = stage->animObjects + a->object;
        // Flixel play(name, force = false): mesma animacao rodando (ou em loop) nao reinicia.
        if (!a->forced && o->currentAnim.isValid && o->currentAnim.animationIndex == a->anim &&
            (o->animationSet.animations[a->anim].looped || !AnimatedObject_FinishedAnimation(o)))
            continue;
        AnimatedObject_SetAnimation(o, a->anim);
    }
}

// Sprites do .lua na ordem do addLuaSprite.
static void DrawItems(PlayStateStage* stage, int from, int to, char paused)
{
    for (int i = from; i < to; i++) {
        const PlayStateStageItem* item = stage->items + i;
        if (item->animated) {
            RayAnimatedObject* o = stage->animObjects + item->index;
            if (!paused) AnimatedObject_UpdateFrame(o);
            Render_DrawAnimatedObject(o);
        } else {
            Render_DrawGraphicObject(stage->objects + item->index);
        }
    }
}

void PlayStateStage_DrawBack(PlayStateStage* stage, char paused)
{
    /* Ondulacao do fundo (evento "Shader Wave"): so esta camada, fora da pilha de shaders. */
    const int wave = ShaderFx_BeginBackgroundWave();

    if (stage->items != NULL) {
        DrawItems(stage, 0, stage->itemBack, paused);
    } else {
        for (int i = 0; i < stage->backgroundObjects; i++)
            Render_DrawGraphicObject(stage->objects + i);
    }

    if (wave) ShaderFx_EndBackgroundWave();
}

void PlayStateStage_DrawFront(PlayStateStage* stage, char paused)
{
    if (stage->items != NULL) {
        DrawItems(stage, stage->itemBack, stage->itemCount, paused);
        return;
    }
    for (int i = stage->backgroundObjects; i < stage->objectCount; i++)
        Render_DrawGraphicObject(stage->objects + i);
}

void PlayStateStage_Unload(PlayStateStage* stage)
{
    if (activeStage == stage) activeStage = NULL;

    // Retangulos nao tem textura (rectSize divide a union com image).
    for (int i = 0; i < stage->objectCount; i++)
        if (stage->objects[i].objType == RGT_IMAGE) UnloadTexture(stage->objects[i].image);

    for (int i = 0; i < stage->animCount; i++)
        AnimationSet_FreeAll(&stage->animObjects[i].animationSet);

    free(stage->objects);
    free(stage->animObjects);
    free(stage->items);
    free(stage->actions);
    memset(stage, 0, sizeof(*stage));
}
