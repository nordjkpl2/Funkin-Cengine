/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateDeath.c - tela de morte do PlayState.
 */


#include "characters/Character.h"
#include "scenes/gameplay/PlayStateDeath.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "scenes/gameplay/PlayStateCamera.h"
#include "raymath.h"

static int deathAnimations[3];
static float deadTime;
static char justDied;
static char retryin;
static Music deathMusic, deathMusicEnd;
static Sound deathSounds;

static float smooth(float a, float b, float c) {
    return Lerp(a, b, c * c * c * c);
}

void PlayStateDeath_LoadAnimations(Character* bf) {
    Character_LoadDeathAnimations(bf, deathAnimations);
}

void PlayStateDeath_LoadSounds(void) {
    deathSounds = LoadSound("assets/sounds/fnf_loss_sfx.ogg");
    deathMusic = LoadMusicStream("assets/music/gameOver.ogg");
    deathMusic.looping = 1;
    deathMusicEnd = LoadMusicStream("assets/music/gameOverEnd.ogg");
    deathMusicEnd.looping = 0;
}

void PlayStateDeath_SetVolume(float volume) {
    if (deathMusic.stream.buffer != NULL)
        SetMusicVolume(deathMusic, volume);

    if (deathMusicEnd.stream.buffer != NULL)
        SetMusicVolume(deathMusicEnd, volume);
}

void PlayStateDeath_Reset(void) {
    deadTime = 0;
    justDied = 1;
    retryin = 0;
}

void PlayStateDeath_Stop(void) {
    StopMusicStream(deathMusic);
    StopMusicStream(deathMusicEnd);
    StopSound(deathSounds);
}

PlayStateDeathAction PlayStateDeath_Update(float dt, Character* bf) {
    PlayStateAudio_Pause();

    if (justDied) {
        AnimatedObject_SetAnimation(&bf->object, deathAnimations[0]);
        deadTime = -AnimationSet_AnimationLength(&bf->object.animationSet, deathAnimations[0]);
        justDied = 0;
        retryin = 0;
        PlaySound(deathSounds);
    }

    PlayStateCamera_Follow(bf);

    if (retryin) {
        UpdateMusicStream(deathMusicEnd);
        bf->object.color.a = smooth(255.0f, 0, deadTime / 2);

        if (deadTime >= 2.0f) {
            StopMusicStream(deathMusicEnd);
            return PLAYSTATE_DEATH_RESTART;
        }
    }
    else if (deadTime > 0) {
        if (bf->object.currentAnim.animationIndex != deathAnimations[1]) {
            PlayMusicStream(deathMusic);
            AnimatedObject_SetAnimation(&bf->object, deathAnimations[1]);
        }
        else {
            UpdateMusicStream(deathMusic);

            if (IsKeyPressed(KEY_SPACE)) {
                retryin = 1;
                deadTime = 0;
                AnimatedObject_SetAnimation(&bf->object, deathAnimations[2]);
                StopMusicStream(deathMusic);
                PlayMusicStream(deathMusicEnd);
            }
            else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
                StopMusicStream(deathMusic);
                return PLAYSTATE_DEATH_LEAVE;
            }
        }
    }

    deadTime += dt;
    return PLAYSTATE_DEATH_NONE;
}
