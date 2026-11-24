/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateCameraMove.c - evento "Camera Move". Transicao exponencial (sem salto).
 */
#include "scenes/gameplay/events/PlayStateCameraMove.h"
#include <math.h>

#define CAMERA_MOVE_EASE 2.5f   /* velocidade da transicao (1/s) */

static float zoomNow = 1.0f;
static float zoomGoal = 1.0f;
static Vector2 offNow;
static Vector2 offGoal;

void PlayStateCameraMove_Reset(void) {
    zoomNow = zoomGoal = 1.0f;
    offNow = offGoal = (Vector2){0.0f, 0.0f};
}

void PlayStateCameraMove_Set(float zoomPct, float x, float y) {
    zoomGoal = zoomPct > 0.0f ? zoomPct / 100.0f : 1.0f;
    offGoal = (Vector2){x, y};
}

void PlayStateCameraMove_Update(float deltaTime) {
    const float k = 1.0f - expf(-CAMERA_MOVE_EASE * (deltaTime > 0.0f ? deltaTime : 0.0f));
    zoomNow += (zoomGoal - zoomNow) * k;
    offNow.x += (offGoal.x - offNow.x) * k;
    offNow.y += (offGoal.y - offNow.y) * k;
}

float PlayStateCameraMove_Zoom(void) {
    return zoomNow;
}

Vector2 PlayStateCameraMove_Offset(void) {
    return offNow;
}
