/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCamera.c - cameras de jogo e HUD do PlayState.
 */

#include "scenes/gameplay/visual/PlayStateCamera.h"
#include "raymath.h"
#include "scenes/gameplay/events/PlayStateParallax.h"
#include "scenes/gameplay/events/PlayStateCameraMove.h"
#include "data/config/EffectsConfig.h"

#define BOP_DEFAULT_BEATS 4

static Character* followTarget;
static Camera2D camGame;
static Camera2D camHUD;

static float followSpeed = 1.0f;

static Camera2D parallaxCam; /* copia da camera de jogo com o parallax aplicado */
static Camera2D shakeGameCam; /* copias com o tremor aplicado (Render_SetCamera guarda o ponteiro) */
static Camera2D shakeHudCam;

/* Estado dos eventos (ResetEvents). */
static char focusForced;
static Vector2 focusPos;
static float stageZoom = 1.0f;     /* defaultGameZoom do ultimo Update */
static float zoomOverride;         /* 0 = o do stage */
static float speedOverride;        /* 0 = o do stage */
static int bopEvery = BOP_DEFAULT_BEATS;
static float bopMult = 1.0f;
static char bopOn = 1;
typedef struct {
    float intensity;
    float timeLeft;
    Vector2 offset;   /* desta frame, em px do mundo */
} Shake;
static Shake shakeGame, shakeHud;

void PlayStateCamera_SetSpeed(float speed) {
    followSpeed = speed > 0.0f ? speed : 1.0f;
}

void PlayStateCamera_ResetEvents(void) {
    focusForced = 0;
    zoomOverride = 0.0f;
    speedOverride = 0.0f;
    bopEvery = BOP_DEFAULT_BEATS;
    bopMult = 1.0f;
    bopOn = 1;
    shakeGame = (Shake){0};
    shakeHud = (Shake){0};
}

void PlayStateCamera_Reset(Character* initialTarget) {
    followSpeed = 1.0f;
    PlayStateParallax_Reset();
    PlayStateCameraMove_Reset();
    PlayStateCamera_ResetEvents();
    followTarget = initialTarget;

    camGame = (Camera2D) {
        .target = (Vector2) {0.0f, 0.0f},
        .rotation = 0,
        .zoom = 1
    };

    camHUD = (Camera2D) {
        .target = (Vector2) {1280.0 / 2, 720.0 / 2},
        .rotation = 0,
        .zoom = 1
    };
}

void PlayStateCamera_Follow(Character* target) {
    followTarget = target;
}

void PlayStateCamera_SetPosition(float x, float y) {
    camGame.target.x = x;
    camGame.target.y = y;
}

void PlayStateCamera_SetGameZoom(float zoom) {
    camGame.zoom = zoom;
}

void PlayStateCamera_Bump(float hudZoom, float gameZoom) {
    camHUD.zoom += hudZoom;
    camGame.zoom += gameZoom;
}

void PlayStateCamera_BeatBump(int beat, float hudFactor, float gameFactor) {
    if (!bopOn || bopEvery <= 0 || beat % bopEvery != 0)
        return;
    PlayStateCamera_Bump(0.03f * hudFactor * bopMult, 0.015f * gameFactor * bopMult);
}

void PlayStateCamera_ForceFocus(char on, float x, float y) {
    focusForced = on;
    focusPos = (Vector2){x, y};
}

Vector2 PlayStateCamera_Target(void) {
    return camGame.target;
}

void PlayStateCamera_Shake(char hud, float intensity, float seconds) {
    Shake* s = hud ? &shakeHud : &shakeGame;
    s->intensity = intensity;
    s->timeLeft = seconds;
}

void PlayStateCamera_SetZoomOverride(float zoom) {
    zoomOverride = zoom > 0.0f ? zoom : 0.0f;
}

float PlayStateCamera_BaseZoom(void) {
    return zoomOverride > 0.0f ? zoomOverride : stageZoom;
}

float PlayStateCamera_GameZoom(void) {
    return camGame.zoom;
}

void PlayStateCamera_SetSpeedOverride(float speed) {
    speedOverride = speed > 0.0f ? speed : 0.0f;
}

void PlayStateCamera_SetBop(int everyBeats, float mult) {
    bopEvery = everyBeats > 0 ? everyBeats : 0;
    bopMult = mult;
}

void PlayStateCamera_SetBopEnabled(char on) {
    bopOn = on;
}

/* Flixel: deslocamento aleatorio de ate intensidade * tamanho da tela, novo a cada frame. */
static void UpdateShake(Shake* s, float deltaTime) {
    if (s->timeLeft <= 0.0f) {
        s->offset = (Vector2){0.0f, 0.0f};
        return;
    }
    s->timeLeft -= deltaTime;
    const float rx = (float)GetRandomValue(-1000, 1000) / 1000.0f;
    const float ry = (float)GetRandomValue(-1000, 1000) / 1000.0f;
    s->offset = (Vector2){rx * s->intensity * 1280.0f, ry * s->intensity * 720.0f};
}

void PlayStateCamera_Update(float deltaTime, float defaultGameZoom) {
    stageZoom = defaultGameZoom;
    // Camera base (Effects Editor) + evento Camera Move, somados ao seguir o personagem.
    const EffectsCamera* base = &EffectsConfig_Get()->camera;
    const Vector2 move = PlayStateCameraMove_Offset();

    Vector2 goal;
    if (focusForced) {
        goal = (Vector2){focusPos.x + move.x, focusPos.y + move.y};   /* Camera Follow Pos */
    } else {
        goal = followTarget->object.position;
        goal.x += followTarget->cameraOffset.x + (float)base->offsetX + move.x;
        goal.y += followTarget->cameraOffset.y + (float)base->offsetY + move.y;
    }

    const float speed = speedOverride > 0.0f ? speedOverride : followSpeed;
    camGame.target.x = Lerp(camGame.target.x, goal.x, deltaTime * 5 * speed);
    camGame.target.y = Lerp(camGame.target.y, goal.y, deltaTime * 5 * speed);
    const float zoomGoal = PlayStateCamera_BaseZoom() * ((float)base->zoomPct / 100.0f) * PlayStateCameraMove_Zoom();
    camGame.zoom = Lerp(camGame.zoom, zoomGoal, deltaTime * 8);
    camHUD.zoom = Lerp(camHUD.zoom, 1, deltaTime * 8);

    UpdateShake(&shakeGame, deltaTime);
    UpdateShake(&shakeHud, deltaTime);
    PlayStateCameraMove_Update(deltaTime);
    PlayStateParallax_Update(deltaTime);
}

/* Copia com o tremor (ou a propria camera, sem tremor). */
static Camera2D* WithShake(Camera2D* cam, const Shake* s, Camera2D* copy) {
    if (s->timeLeft <= 0.0f)
        return cam;
    *copy = *cam;
    copy->target.x += s->offset.x;
    copy->target.y += s->offset.y;
    return copy;
}

void PlayStateCamera_BeginGame(void) {
    Camera2D* cam = WithShake(&camGame, &shakeGame, &shakeGameCam);
    if (PlayStateParallax_Apply(cam, &parallaxCam))
        Render_SetCamera(&parallaxCam);
    else
        Render_SetCamera(cam);
}

void PlayStateCamera_BeginHUD(void) {
    Render_SetCamera(WithShake(&camHUD, &shakeHud, &shakeHudCam));
}
