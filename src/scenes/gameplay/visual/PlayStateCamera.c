/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCamera.c - cameras de jogo e HUD do PlayState.
 */

#include "scenes/gameplay/visual/PlayStateCamera.h"
#include "raymath.h"
#include "scenes/gameplay/events/PlayStateParallax.h"
#include "scenes/gameplay/events/PlayStateCameraMove.h"
#include "data/config/EffectsConfig.h"

static Character* followTarget;
static Camera2D camGame;
static Camera2D camHUD;

static float followSpeed = 1.0f;

static Camera2D parallaxCam; /* copia da camera de jogo com o parallax aplicado */

void PlayStateCamera_SetSpeed(float speed) {
    followSpeed = speed > 0.0f ? speed : 1.0f;
}

void PlayStateCamera_Reset(Character* initialTarget) {
    followSpeed = 1.0f;
    PlayStateParallax_Reset();
    PlayStateCameraMove_Reset();
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

void PlayStateCamera_Update(float deltaTime, float defaultGameZoom) {
    // Camera base (Effects Editor) + evento Camera Move, somados ao seguir o personagem.
    const EffectsCamera* base = &EffectsConfig_Get()->camera;
    const Vector2 move = PlayStateCameraMove_Offset();

    Vector2 goal = followTarget->object.position;
    goal.x += followTarget->cameraOffset.x + (float)base->offsetX + move.x;
    goal.y += followTarget->cameraOffset.y + (float)base->offsetY + move.y;

    camGame.target.x = Lerp(camGame.target.x, goal.x, deltaTime * 5 * followSpeed);
    camGame.target.y = Lerp(camGame.target.y, goal.y, deltaTime * 5 * followSpeed);
    const float zoomGoal = defaultGameZoom * ((float)base->zoomPct / 100.0f) * PlayStateCameraMove_Zoom();
    camGame.zoom = Lerp(camGame.zoom, zoomGoal, deltaTime * 8);
    camHUD.zoom = Lerp(camHUD.zoom, 1, deltaTime * 8);

    PlayStateCameraMove_Update(deltaTime);
    PlayStateParallax_Update(deltaTime);
}

void PlayStateCamera_BeginGame(void) {
    if (PlayStateParallax_Apply(&camGame, &parallaxCam))
        Render_SetCamera(&parallaxCam);
    else
        Render_SetCamera(&camGame);
}

void PlayStateCamera_BeginHUD(void) {
    Render_SetCamera(&camHUD);
}
