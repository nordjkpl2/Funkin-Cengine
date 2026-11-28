/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateChangers.c - Scroll Type/Speed e Instakill on Miss da Psych.
 */

#include "scenes/gameplay/flow/PlayStateChangers.h"
#include <math.h>

static ScrollType scrollType = SCROLL_TYPE_MULT;
static float scrollValue = 1.0f;
static char instakill;

void PlayStateChangers_Reset(void) {
    scrollType = SCROLL_TYPE_MULT;
    scrollValue = 1.0f;
    instakill = 0;
}

ScrollType PlayStateChangers_ScrollType(void) { return scrollType; }
float PlayStateChangers_ScrollValue(void) { return scrollValue; }
char PlayStateChangers_Instakill(void) { return instakill; }

char PlayStateChangers_SetScrollValue(float value) {
    const float v = fminf(fmaxf(roundf(value / CHANGERS_SCROLL_STEP) * CHANGERS_SCROLL_STEP, CHANGERS_SCROLL_MIN),
                          CHANGERS_SCROLL_MAX(scrollType));
    const char changed = v != scrollValue;
    scrollValue = v;
    return changed;
}

char PlayStateChangers_SetScrollType(ScrollType type) {
    if (type == scrollType) return 0;
    scrollType = type;
    PlayStateChangers_SetScrollValue(scrollValue); // Constant -> Multiplicative: corta no novo maximo
    return 1;
}

char PlayStateChangers_SetInstakill(char on) {
    on = on != 0;
    const char changed = on != instakill;
    instakill = on;
    return changed;
}

char PlayStateChangers_IsModified(void) {
    return scrollType != SCROLL_TYPE_MULT || scrollValue != 1.0f || instakill;
}

float PlayStateChangers_SongSpeed(float chartSpeed, float eventMult) {
    // Psych: Change Scroll Speed so age no Multiplicative.
    return scrollType == SCROLL_TYPE_CONST ? scrollValue : chartSpeed * scrollValue * eventMult;
}
