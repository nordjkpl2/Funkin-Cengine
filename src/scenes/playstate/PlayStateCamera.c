/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCamera.c - cameras de jogo e HUD do PlayState.
 */

#include "scenes/playstate/PlayStateCamera.h"

static Character* followTarget;
static Camera2D camGame;
static Camera2D camHUD;

void PlayStateCamera_Reset(Character* initialTarget) {
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
    Vector2 goal = followTarget->object.position;
    goal.x += followTarget->cameraOffset.x;
    goal.y += followTarget->cameraOffset.y;

    camGame.target.x = Lerp(camGame.target.x, goal.x, deltaTime * 5);
    camGame.target.y = Lerp(camGame.target.y, goal.y, deltaTime * 5);
    camGame.zoom = Lerp(camGame.zoom, defaultGameZoom, deltaTime * 8);
    camHUD.zoom = Lerp(camHUD.zoom, 1, deltaTime * 8);
}

void PlayStateCamera_BeginGame(void) {
    Render_SetCamera(&camGame);
}

void PlayStateCamera_BeginHUD(void) {
    Render_SetCamera(&camHUD);
}
