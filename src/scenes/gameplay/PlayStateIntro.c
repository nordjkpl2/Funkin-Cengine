/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateIntro.c - contagem regressiva (ready, set, go) do PlayState.
 */


#include "characters/Character.h"
#include "scenes/gameplay/PlayStateIntro.h"
#include "raymath.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

static int introHit;
static float introTime, introStep;
static RayAnimationHandler uiAssets;
static RayAnimatedObject introText;
static Sound introSounds[4];

static float smooth(float a, float b, float c) {
    return Lerp(a, b, c * c * c * c);
}

void PlayStateIntro_LoadSounds(void) {
    static const char* const introFiles[4] = {
        "assets/sounds/intro3.ogg", "assets/sounds/intro2.ogg", "assets/sounds/intro1.ogg", "assets/sounds/introGo.ogg"
    };

    for (int i = 0; i < 4; i++) {
        introSounds[i] = LoadSound(introFiles[i]);
        SetSoundVolume(introSounds[i], 0.7f);
    }
}

void PlayStateIntro_LoadUi(const RayAnimationHandler* ui) {
    uiAssets = *ui;

    Render_DefaultAnimated(&introText);
    introText.animationSet = uiAssets;
    introText.scaleX = introText.scaleY = 0.7f;
}

float PlayStateIntro_Start(float bpm) {
    introHit = 0;
    introTime = -0.5f;
    introStep = 60.0f / bpm;
    return introTime + -5 * introStep;
}

void PlayStateIntro_Restart(void) {
    introText.currentAnim.isValid = 0;
}

void PlayStateIntro_Update(float dt, Girlfriend* gf) {
    static const char* const countdownAnims[3] = {"ready", "set", "go"};

    introTime += dt;

    while (introTime >= introStep) {
        if (introHit < 4) {
            if (introHit > 0) {
                AnimatedObject_SetAnimation(&introText, AnimationSet_FindAnimation(&uiAssets, countdownAnims[introHit - 1]));
                Vector2 sizes = AnimatedObject_Sizes(&introText);
                introText.position.x = SCREEN_W / 2 - sizes.x / 2;
                introText.position.y = SCREEN_H / 2 - sizes.y / 2;
            }

            PlaySound(introSounds[introHit]);
        }

        if (gf) {
            gf->f = !gf->f;
            AnimatedObject_SetAnimation(&gf->object, gf->anims[gf->f]);
        }

        introHit++;
        introTime -= introStep;
    }

    introText.color.a = smooth(255.0f, 0, introTime / introStep);
}

void PlayStateIntro_Draw(void) {
    Render_DrawAnimatedObject(&introText);
}
