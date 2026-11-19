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
static float bobY;            /* calculado em Update, usado no Draw */
static float sway;
static float minHealth;       /* -inf se mortal (morre abaixo de 0), senao 0 */

/* area interna da barra em pixels inteiros: calculada uma vez no Init */
static int innerX, innerY, innerW, innerH;

void PlayStateHealth_Init(char top, char mortal) {
    minHealth = mortal ? -INFINITY : 0.0f;
    healthbarPadding = 4;
    healthbar.width = 600;
    healthbar.height = 17;
    healthbar.x = 1200.0f / 2 - healthbar.width / 2;
    /* base: y 600 (103 px da borda de baixo); topo: mesma distancia de cima */
    healthbar.y = top ? 103 : 600;

    innerX = (int)healthbar.x + healthbarPadding;
    innerY = (int)healthbar.y + healthbarPadding;
    innerW = (int)healthbar.width - healthbarPadding * 2;
    innerH = (int)healthbar.height - healthbarPadding * 2;

    dadColor = (Color) {204, 85, 180, 255};
    bfColor = (Color) {0, 217, 204, 255};
}

void PlayStateHealth_Reset(void) {
    health = 0.5f;
    bop = 1;
    bopT = 1;
    bobY = 0;
    sway = 0;
}

void PlayStateHealth_SetCharacterColors(const Character* dad, const Character* bf) {
    if (dad->hasHealthColor) dadColor = dad->healthColor;
    if (bf->hasHealthColor) bfColor = bf->healthColor;
}

/* PlayStateHealth_Add ja deixa health em [minHealth, 1]; nada mais precisa limitar depois. */
void PlayStateHealth_Add(float delta) {
    const float h = health + delta;
    health = h > 1.0f ? 1.0f : (h < minHealth ? minHealth : h);
}

/* Drain do oponente: desce no maximo ate 0, sem matar o player (misses ainda podem matar). */
void PlayStateHealth_Drain(float delta) {
    const float h = health - delta;
    if (h >= 0.0f) health = h;
    else if (health > 0.0f) health = 0.0f;
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

    const float k = 1 - bopT;
    const float kk = k * k;
    bop = 1 + 0.22f * kk;
    bobY = -7 * kk;
    sway = 4 * kk;
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

    /* so a morte (health < 0) precisa cortar em 0 */
    const float shown = health < 0 ? 0.0f : health;
    const float middle = innerW * (1.0f - shown);
    const int splitX = innerX + (int)middle;
    const int dadW = splitX - innerX;
    const int bfW = innerX + innerW - splitX;

    if (dadW > 0) DrawRectangle(innerX, innerY, dadW, innerH, dadColor);
    if (bfW > 0)  DrawRectangle(splitX, innerY, bfW, innerH, bfColor);

    const float iconHalf = 75 * bop;
    const float iconCY = healthbar.y + healthbar.height / 2;
    const float iconX = innerX + middle;

    DrawTexturePro(dadIcon, Character_IconFrame(dadIcon), (Rectangle) {
        .x = iconX + C - 75 - iconHalf - sway,
        .y = iconCY - iconHalf + bobY,
        .width = 130 * bop,
        .height = 130 * bop
    }, (Vector2) {0, 0}, 0, WHITE);

    DrawTexturePro(bfIcon, Character_IconFrame(bfIcon), (Rectangle) {
        .x = iconX - C + 75 - iconHalf + sway,
        .y = iconCY - iconHalf - bobY,
        .width = 145 * bop,
        .height = 145 * bop
    }, (Vector2) {0, 0}, 0, WHITE);
}
