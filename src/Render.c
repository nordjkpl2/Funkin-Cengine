/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * Render.c - High-Performance Clean Rendering Engine
 */

#include "Render.h"
#include "Game.h"

#include <math.h>
#include <stdbool.h>

/* Pre-allocated constant vector to avoid struct construction on stack */
static const Vector2 VEC_ZERO = {0.0f, 0.0f};

/* Global Camera Cache State */
static Camera2D *s_target = NULL;
static float s_savedZoom = 1.0f;
static Vector2 s_savedOffset = {0.0f, 0.0f};

/* Cache do Target da Câmera ativo para evitar indireções de ponteiro */
static Vector2 s_activeCamTarget = {0.0f, 0.0f};

/* =========================================================
   CAMERA CONTROL
   ========================================================= */

void Render_SetCamera(Camera2D *camera)
{
    if (camera == NULL)
        return;

    /* Evita fechar/abrir modo 2D se a mesma câmera já estiver ativa */
    if (s_target != NULL)
    {
        if (s_target == camera)
        {
            s_activeCamTarget = camera->target;
            return;
        }
        Render_StopCamera();
    }

    s_target = camera;
    s_savedZoom = camera->zoom;
    s_savedOffset = camera->offset;
    s_activeCamTarget = camera->target;

    camera->zoom = s_savedZoom * RayGame_ZoomFactor();
    camera->offset = (Vector2){
        RayGame_CanvasWidth() * 0.5f,
        RayGame_CanvasHeight() * 0.5f
    };

    BeginMode2D(*camera);
}

void Render_StopCamera(void)
{
    if (s_target != NULL)
    {
        s_target->zoom = s_savedZoom;
        s_target->offset = s_savedOffset;
        s_target = NULL;
        s_activeCamTarget = VEC_ZERO;

        EndMode2D();
    }
}

void Render_DefaultCamera(Camera2D *cam)
{
    if (cam == NULL)
        return;

    cam->zoom = 1.0f;
    cam->target = (Vector2){640.0f, 360.0f};
    cam->offset = VEC_ZERO;
    cam->rotation = 0.0f;
}

/* =========================================================
   TEXTURES
   ========================================================= */

Texture2D Render_LoadTexture(const char *path)
{
    Texture2D texture = LoadTexture(path);

    if (texture.id != 0)
    {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }

    return texture;
}

/* =========================================================
   SCROLL OFFSET
   ========================================================= */

static Vector2 Render_GetScrollOffset(const Vector2 scrollFactor)
{
    if (s_target == NULL)
        return VEC_ZERO;

    /* HUD e elementos estáticos evitam matemática desnecessária */
    if (scrollFactor.x == 1.0f && scrollFactor.y == 1.0f)
        return VEC_ZERO;

    const float tx = s_target->target.x;
    const float ty = s_target->target.y;

    return (Vector2){
        (1.0f - scrollFactor.x) * tx,
        (1.0f - scrollFactor.y) * ty
    };
}

/* =========================================================
   GRAPHIC OBJECT RENDERING
   ========================================================= */

void Render_DrawGraphicObject(const RayGraphicObject *restrict obj)
{
    if (obj == NULL || obj->color.a == 0)
        return;

    const Vector2 offset = Render_GetScrollOffset(obj->scrollFactor);
    const float x = obj->position.x + offset.x;
    const float y = obj->position.y + offset.y;

    switch (obj->objType)
    {
        case RGT_RECTANGLE:
        {
            const Rectangle rect = {
                x,
                y,
                obj->rectSize.x * obj->scaleX,
                obj->rectSize.y * obj->scaleY
            };

            DrawRectanglePro(rect, VEC_ZERO, obj->rotation, obj->color);
            break;
        }

        case RGT_CIRCLE:
        {
            DrawCircleV((Vector2){x, y}, obj->radius * obj->scaleX, obj->color);
            break;
        }

        case RGT_IMAGE:
        {
            if (obj->image.id == 0)
                break;

            const float texW = (float)obj->image.width;
            const float texH = (float)obj->image.height;

            const Rectangle source = { 0.0f, 0.0f, texW, texH };
            const Rectangle destination = { x, y, texW * obj->scaleX, texH * obj->scaleY };

            DrawTexturePro(
                obj->image,
                source,
                destination,
                VEC_ZERO,
                obj->rotation,
                obj->color
            );
            break;
        }

        default:
            break;
    }
}

/* =========================================================
   ANIMATED OBJECT RENDERING
   ========================================================= */

void Render_DrawAnimatedObject(const RayAnimatedObject *restrict obj)
{
    if (obj == NULL || !obj->currentAnim.isValid || obj->color.a == 0)
        return;

    const RayAnimation *anim = &obj->animationSet.animations[obj->currentAnim.animationIndex];
    const unsigned int curFrame = obj->currentAnim.currentFrame;

    if (anim->frames == NULL || anim->frameCount <= 0 || curFrame >= (unsigned int)anim->frameCount)
        return;

    const Frame *frame = &anim->frames[curFrame];
    const Vector2 offset = Render_GetScrollOffset(obj->scrollFactor);

    const float scaleX = obj->scaleX;
    const float scaleY = obj->scaleY;

    const float frameW = (float)frame->w;
    const float frameH = (float)frame->h;

    const Rectangle source = {
        (float)frame->x,
        (float)frame->y,
        frameW,
        frameH
    };

    const Rectangle destination = {
        obj->position.x + offset.x + (-anim->animationOffset.x - frame->fx) * scaleX,
        obj->position.y + offset.y + (-anim->animationOffset.y - frame->fy) * scaleY,
        frameW * scaleX,
        frameH * scaleY
    };

    DrawTexturePro(
        obj->animationSet.texture,
        source,
        destination,
        VEC_ZERO,
        obj->rotation,
        obj->color
    );
}

/* =========================================================
   OBJECT SIZES
   ========================================================= */

Vector2 GraphicObject_Sizes(const RayGraphicObject *restrict obj)
{
    if (obj == NULL)
        return VEC_ZERO;

    Vector2 size;

    switch (obj->objType)
    {
        case RGT_RECTANGLE:
            size = obj->rectSize;
            break;

        case RGT_CIRCLE:
            size.x = size.y = obj->radius * 2.0f;
            break;

        case RGT_IMAGE:
        default:
            size.x = (float)obj->image.width;
            size.y = (float)obj->image.height;
            break;
    }

    size.x *= obj->scaleX;
    size.y *= obj->scaleY;

    return size;
}

Vector2 AnimatedObject_Sizes(const RayAnimatedObject *restrict obj)
{
    if (obj == NULL || !obj->currentAnim.isValid)
        return VEC_ZERO;

    const RayAnimation *anim = &obj->animationSet.animations[obj->currentAnim.animationIndex];
    const unsigned int curFrame = obj->currentAnim.currentFrame;

    if (anim->frames == NULL || anim->frameCount <= 0 || curFrame >= (unsigned int)anim->frameCount)
        return VEC_ZERO;

    const Frame *frame = &anim->frames[curFrame];

    return (Vector2){
        (float)frame->w * obj->scaleX,
        (float)frame->h * obj->scaleY
    };
}

/* =========================================================
   ANIMATION UPDATE
   ========================================================= */

void AnimatedObject_UpdateFrame(RayAnimatedObject *restrict obj)
{
    if (obj == NULL || !obj->currentAnim.isValid)
        return;

    RayAnimation *anim = &obj->animationSet.animations[obj->currentAnim.animationIndex];
    const int rawFrameCount = anim->frameCount;

    if (rawFrameCount <= 0 || anim->frames == NULL)
        return;

    const float frameTime = anim->frameTime;
    const float deltaTime = RayGame_DeltaTime();

    if (frameTime <= 0.0f || deltaTime <= 0.0f || deltaTime >= 1.0f)
        return;

    const unsigned int frameCount = (unsigned int)rawFrameCount;
    const unsigned int current = obj->currentAnim.currentFrame;

    if (current >= frameCount)
    {
        obj->currentAnim.currentFrame = 0;
        obj->currentAnim.time = 0.0f;
        return;
    }

    /* Animação não-loopada terminada */
    if (!anim->looped && current == frameCount - 1u)
    {
        obj->currentAnim.time = 0.0f;
        return;
    }

    const float elapsed = obj->currentAnim.time + deltaTime;

    /* FAST-PATH 1: Não deu tempo de mudar de frame */
    if (elapsed < frameTime)
    {
        obj->currentAnim.time = elapsed;
        return;
    }

    /* FAST-PATH 2: Avançou 1 frame (Subtração simples em vez de floorf/fmodf) */
    const float nextTime = elapsed - frameTime;

    if (nextTime < frameTime)
    {
        if (anim->looped)
        {
            obj->currentAnim.currentFrame = (current + 1u >= frameCount) ? 0u : current + 1u;
        }
        else
        {
            obj->currentAnim.currentFrame = current + 1u;
        }
        obj->currentAnim.time = nextTime;
        return;
    }

    /* FALLBACK: Multiplicação por recíproca em caso de lag spike */
    const float invFrameTime = 1.0f / frameTime;
    const unsigned int steps = (unsigned int)(elapsed * invFrameTime);
    const float remainder = elapsed - ((float)steps * frameTime);

    if (anim->looped)
    {
        obj->currentAnim.currentFrame = (current + steps) % frameCount;
        obj->currentAnim.time = remainder;
    }
    else
    {
        const unsigned int remaining = (frameCount - 1u) - current;
        if (steps >= remaining)
        {
            obj->currentAnim.currentFrame = frameCount - 1u;
            obj->currentAnim.time = 0.0f;
        }
        else
        {
            obj->currentAnim.currentFrame = current + steps;
            obj->currentAnim.time = remainder;
        }
    }
}

/* =========================================================
   ANIMATION STATE
   ========================================================= */

char AnimatedObject_FinishedAnimation(const RayAnimatedObject *restrict obj)
{
    if (obj == NULL || !obj->currentAnim.isValid)
        return 0;

    const RayAnimation *anim = &obj->animationSet.animations[obj->currentAnim.animationIndex];

    if (anim->frameCount <= 0)
        return 0;

    return obj->currentAnim.currentFrame >= (unsigned int)(anim->frameCount - 1);
}

void AnimatedObject_SetAnimation(RayAnimatedObject *restrict obj, int idx)
{
    if (obj == NULL)
        return;

    bool valid = (idx >= 0) &&
                 (idx < obj->animationSet.animationCount) &&
                 (obj->animationSet.animations[idx].frameCount > 0) &&
                 (obj->animationSet.animations[idx].frames != NULL);

    if (valid)
    {
        const float ft = obj->animationSet.animations[idx].frameTime;
        valid = (ft > 0.0f) && isfinite(ft);
    }

    obj->currentAnim = (RayAnimationState){
        .isValid = valid,
        .animationIndex = valid ? idx : 0,
        .currentFrame = 0,
        .time = 0.0f
    };
}
