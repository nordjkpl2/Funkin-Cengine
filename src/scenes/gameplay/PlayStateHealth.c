/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateHealth.c - vida, barra de vida e icones do PlayState.
 */

#include "scenes/gameplay/PlayStateHealth.h"
#include <math.h>

static float health;          /* 0 - 1; abaixo de 0 = morte */
static Rectangle healthbar;
static int healthbarPadding;
static Color dadColor;
static Color bfColor;
static float bop;
static float bopT;

void PlayStateHealth_Init(char top) {
    healthbarPadding = 4;
    healthbar.width = 600;
    healthbar.height = 17;
    healthbar.x = 1200.0f / 2 - healthbar.width / 2;
    /* base: y 600 (103 px da borda de baixo); topo: mesma distancia de cima */
    healthbar.y = top ? 103 : 600;

    dadColor = (Color) {204, 85, 180, 255};
    bfColor = (Color) {0, 217, 204, 255};
}

void PlayStateHealth_Reset(void) {
    health = 0.5f;
    bop = 1;
    bopT = 1;
}

void PlayStateHealth_SetCharacterColors(const Character* dad, const Character* bf) {
    if (dad->hasHealthColor) dadColor = dad->healthColor;
    if (bf->hasHealthColor) bfColor = bf->healthColor;
}

void PlayStateHealth_Set(float value) {
    health = value;
}

void PlayStateHealth_Add(float delta) {
    health = fminf(1, health + delta);
}

char PlayStateHealth_IsDead(void) {
    return health < 0;
}

void PlayStateHealth_Bop(void) {
    bopT = 0;
}

void PlayStateHealth_Update(float deltaTime) {
    if (bopT < 1) {
        bopT += deltaTime * 3.5f;
        if (bopT > 1) bopT = 1;
    }

    float k = 1 - bopT;
    bop = 1 + 0.22f * k * k;
}

float PlayStateHealth_BarY(void) {
    return healthbar.y;
}

float PlayStateHealth_CenterX(float width) {
    return healthbar.x + (healthbar.width - width) * 0.5f;
}

void PlayStateHealth_Draw(Texture2D dadIcon, Texture2D bfIcon) {
    const int C = 35;
    DrawRectangleRec(healthbar, BLACK);

    float offsetX = healthbar.x + healthbarPadding;
    float availableWidth = healthbar.width - healthbarPadding * 2;
    float availableHeight = healthbar.height - healthbarPadding * 2;
    float middle = availableWidth * (1 - health);

    if (middle > 0) {
        DrawRectangle((int)offsetX, (int)(healthbar.y + healthbarPadding), (int)middle, (int)availableHeight, dadColor);
    }
    if (middle < availableWidth) {
        DrawRectangle((int)(offsetX + middle), (int)(healthbar.y + healthbarPadding), (int)(availableWidth - middle), (int)availableHeight, bfColor);
    }

    float iconHalf = 75 * bop;
    float iconCY = healthbar.y + healthbar.height / 2;
    float kk = 1 - bopT;
    kk *= kk;
    float bobY = -7 * kk;
    float sway = 4 * kk;

    DrawTexturePro(dadIcon, Character_IconFrame(dadIcon), (Rectangle) {
        .x = offsetX + middle + C - 75 - iconHalf - sway,
        .y = iconCY - iconHalf + bobY,
        .width = 130 * bop,
        .height = 130 * bop
    }, (Vector2) {0, 0}, 0, WHITE);

    DrawTexturePro(bfIcon, Character_IconFrame(bfIcon), (Rectangle) {
        .x = offsetX + middle - C + 75 - iconHalf + sway,
        .y = iconCY - iconHalf - bobY,
        .width = 145 * bop,
        .height = 145 * bop
    }, (Vector2) {0, 0}, 0, WHITE);
}
