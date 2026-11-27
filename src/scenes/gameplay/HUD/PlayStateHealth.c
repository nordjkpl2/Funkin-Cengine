/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateHealth.c - vida, barra de vida e icones do PlayState.
 */

#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "GameData.h"
#include "data/config/HudConfig.h"
#include <float.h>
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
/* Play as oponente (sem Co-Op): "health" e a vida do jogador, que fica do lado do oponente na barra. */
static char healthOnDad;
static bool healthHidden;     /* evento Health Bar = 0: nao desenha barra nem icones */
static bool layoutHidden;     /* HUD Editor: barra escondida no layout */
static float iconScale;       /* HUD Editor: escala dos icones (1 = padrao) */

/* area interna da barra em pixels inteiros: calculada uma vez no Init */
static int innerX, innerY, innerW, innerH;

void PlayStateHealth_Init(char top, char mortal) {
    minHealth = mortal ? -FLT_MAX : 0.0f;   /* mortal: limite inferior finito (morre abaixo de 0) */
    healthOnDad = gameOptions.playAsOpponent && !gameOptions.coopOn && !gameOptions.botplay;
    healthbarPadding = 4;
    /* layout do HUD Editor: largura, deslocamento, escala dos icones e visibilidade */
    const HudConfig* hud = HudConfig_Get();
    const HudElement* el = &hud->el[HUD_EL_HEALTH];
    healthbar.width = roundf(600.0f * (float)hud->barWidthPct / 100.0f);
    healthbar.height = 17;
    healthbar.x = 1280.0f / 2 - healthbar.width / 2 + (float)el->offsetX;   /* centro da tela */
    /* base: y 600 (103 px da borda de baixo); topo: mesma distancia de cima */
    healthbar.y = (top ? 103.0f : 600.0f) + (float)el->offsetY;
    iconScale = (float)el->scalePct / 100.0f;
    layoutHidden = !el->visible;

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

void PlayStateHealth_AddForSide(float delta, char bfSide) {
    PlayStateHealth_Add(gameOptions.coopOn && !bfSide ? -delta : delta);
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

/* Opcao Losing Icon: o icone troca para o de derrota quando o lado esta perdendo (so visual). */
#define ICON_LOSE_BF 0.2f   /* BF perdendo: vida abaixo disso */
#define ICON_LOSE_DAD 0.8f  /* oponente perdendo: vida acima disso */

void PlayStateHealth_SetVisible(bool visible) {
    healthHidden = !visible;
}

void PlayStateHealth_Draw(Texture2D dadIcon, Texture2D bfIcon) {
    if (healthHidden || layoutHidden) return;
    const int C = 35;
    /* vida do BF (a barra e desenhada desse ponto de vista); com Play as oponente, a do BF e o inverso */
    const float bfHealth = healthOnDad ? 1.0f - health : health;
    const bool dadLosing = gameOptions.loseIcon && bfHealth > ICON_LOSE_DAD;
    const bool bfLosing = gameOptions.loseIcon && bfHealth < ICON_LOSE_BF;
    DrawRectangleRec(healthbar, BLACK);

    /* so a morte (vida < 0) precisa cortar em 0 */
    const float shown = bfHealth < 0 ? 0.0f : (bfHealth > 1.0f ? 1.0f : bfHealth);
    const float middle = innerW * (1.0f - shown);
    const int splitX = innerX + (int)middle;
    const int dadW = splitX - innerX;
    const int bfW = innerX + innerW - splitX;

    if (dadW > 0) DrawRectangle(innerX, innerY, dadW, innerH, dadColor);
    if (bfW > 0)  DrawRectangle(splitX, innerY, bfW, innerH, bfColor);

    const float ib = bop * iconScale;
    const float iconHalf = 75 * ib;
    const float iconCY = healthbar.y + healthbar.height / 2;
    const float iconX = innerX + middle;

    DrawTexturePro(dadIcon, dadLosing ? Character_IconLoseFrame(dadIcon) : Character_IconFrame(dadIcon), (Rectangle) {
        .x = iconX + (C - 75) * iconScale - iconHalf - sway,
        .y = iconCY - iconHalf + bobY,
        .width = 130 * ib,
        .height = 130 * ib
    }, (Vector2) {0, 0}, 0, WHITE);

    DrawTexturePro(bfIcon, bfLosing ? Character_IconLoseFrame(bfIcon) : Character_IconFrame(bfIcon), (Rectangle) {
        .x = iconX + (75 - C) * iconScale - iconHalf + sway,
        .y = iconCY - iconHalf - bobY,
        .width = 145 * ib,
        .height = 145 * ib
    }, (Vector2) {0, 0}, 0, WHITE);
}
