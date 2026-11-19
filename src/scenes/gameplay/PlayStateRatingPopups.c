/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateRatingPopups.c - popup de rating do PlayState.
 */

#include "scenes/AllScenes.h"
#include "scenes/gameplay/PlayStateRatingPopups.h"
#include "scenes/gameplay/PlayStateHealth.h"
#include "GameData.h"
#include "raymath.h"

static RayAnimationHandler uiAssets;
static char uiAssetsLoaded;
static int ratingAnim[4];           /* indexado por PlayerRating */
static RayAnimatedObject popupObj;
static float popupTimer;
static float popupVelocity;

const RayAnimationHandler* PlayStateRatingPopups_LoadUi(void) {
    if (!uiAssetsLoaded) {
        uiAssets = AnimationSet_LoadAnimations("assets/images/ui.animset", Render_LoadTexture("assets/images/ui.png"));

        ratingAnim[PLAYER_RATING_SICK] = AnimationSet_FindAnimation(&uiAssets, "sick");
        ratingAnim[PLAYER_RATING_GOOD] = AnimationSet_FindAnimation(&uiAssets, "good");
        ratingAnim[PLAYER_RATING_BAD]  = AnimationSet_FindAnimation(&uiAssets, "bad");
        ratingAnim[PLAYER_RATING_SHIT] = AnimationSet_FindAnimation(&uiAssets, "shit");
        uiAssetsLoaded = 1;
    }

    Render_DefaultAnimated(&popupObj);
    popupObj.animationSet = uiAssets;
    popupObj.scaleX = popupObj.scaleY = 0.6f;

    return &uiAssets;
}

void PlayStateRatingPopups_Reset(void) {
    popupTimer = 0.0f;
}

void PlayStateRatingPopups_Show(PlayerRating rating) {
    if (gameOptions.botplay)
        return;

    AnimatedObject_SetAnimation(&popupObj, ratingAnim[rating]);

    const Vector2 size = AnimatedObject_Sizes(&popupObj);
    popupObj.position.x = PlayStateHealth_CenterX(size.x);
    popupObj.position.y = PlayStateHealth_BarY() + GetRandomValue(-1000, 1000) / 200.0f;
    popupVelocity = -150.0f;
    popupTimer = 1.0f;
}

void PlayStateRatingPopups_Draw(float dt) {
    if (popupTimer <= 0.0f)
        return;

    popupVelocity += 500.0f * dt;
    popupObj.position.y += popupVelocity * dt;

    /* fade: Lerp(255, 0, t^4) com t = 1 - popupTimer (mesma curva de antes, sem pow) */
    const float t = 1.0f - popupTimer;
    const float t2 = t * t;
    popupObj.color.a = Lerp(255.0f, 0.0f, t2 * t2);

    Render_DrawAnimatedObject(&popupObj);
    popupTimer -= dt;
}
