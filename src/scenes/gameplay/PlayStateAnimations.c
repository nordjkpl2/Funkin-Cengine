/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAnimations.c - personagens do PlayState (oponente, BF, GF).
 */

#include "scenes/gameplay/PlayStateAnimations.h"
#include "scenes/gameplay/PlayStateDeath.h"
#include "scenes/gameplay/PlayStateHealth.h"
#include "scenes/gameplay/PlayStateStage.h"
#include "GameData.h"
#include <stddef.h>
#include <string.h>

static Girlfriend gf;
static char gfLoaded;   /* GF carregada nesta musica (descarregar so se sim) */
static char gfHidden;   /* GF nao e desenhada (chart, stage ou "empty"); pode estar carregada */
static Character bf;
static Character dad;

static Sound missSFX[3];
static Sound hitSFX;

static const char* const noGfNames[] = {
    "empty", "none", "nogf", "no-gf", "nogirlfriend", "no_gf", "__not_have_a_gf", ""
};

Character* PlayStateAnimations_Dad(void) { return &dad; }
Character* PlayStateAnimations_Bf(void)  { return &bf; }
Girlfriend* PlayStateAnimations_Gf(void) { return &gf; }
Girlfriend* PlayStateAnimations_GfLoaded(void) { return gfLoaded ? &gf : NULL; }

void PlayStateAnimations_Prefetch(const Song* song, const char* songDir, const char* opponent) {
    /* GF escondida no chart (gfVersion "empty"/"none"...) ou no stage: nao carrega nada dela. */
    gfHidden = 0;
    for (size_t i = 0; i < sizeof(noGfNames) / sizeof(noGfNames[0]); i++) {
        if (strcmp(song->gfVersion, noGfNames[i]) == 0) {
            gfHidden = 1;
            break;
        }
    }
    if (!gfHidden && PlayStateStage_HidesGirlfriend(song->stage, songDir))
        gfHidden = 1;

    Character_SetSongDir(songDir);  /* personagens do mod da musica */

    if (!gfHidden)
        Character_PrefetchTexture(song->gfVersion, CHAR_GF);
    Character_PrefetchTexture(opponent, CHAR_DAD);
    Character_PrefetchTexture(song->player1, CHAR_BF);
}

void PlayStateAnimations_Load(const Song* song, const char* opponent) {
    if (gfHidden) {
        /* Objeto vazio: o stage so soma posicao nele; nada e desenhado nem animado. */
        Render_DefaultAnimated(&gf.object);
        gf.f = 0;
        gf.anims[0] = gf.anims[1] = 0;
        gfLoaded = 0;
    } else {
        Girlfriend_Load(&gf, song->gfVersion);
        gfLoaded = 1;
    }

    Character_Load(&dad, opponent, CHAR_DAD);
    Character_Load(&bf, song->player1, CHAR_BF);
    PlayStateDeath_LoadAnimations(&bf);
    PlayStateHealth_SetCharacterColors(&dad, &bf);
}

void PlayStateAnimations_LoadSounds(void) {
    static const char* const missFiles[3] = {
        "assets/sounds/missnote1.ogg", "assets/sounds/missnote2.ogg", "assets/sounds/missnote3.ogg"
    };

    for (int i = 0; i < 3; i++) {
        missSFX[i] = LoadSound(missFiles[i]);
        SetSoundVolume(missSFX[i], 0.4f);
    }

    hitSFX = LoadSound("assets/sounds/hitosu!mania.ogg");
    SetSoundVolume(hitSFX, 0.5f);
}

void PlayStateAnimations_Restart(void) {
    AnimatedObject_SetAnimation(&dad.object, dad.animations.idle);
    AnimatedObject_SetAnimation(&bf.object, bf.animations.idle);
    if (gfLoaded)
        AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);

    bf.object.color.a = 255;
}

void PlayStateAnimations_HideGirlfriend(void) {
    gfHidden = 1;
}

void PlayStateAnimations_Unload(void) {
    UnloadTexture(dad.icon);
    UnloadTexture(bf.icon);

    AnimationSet_FreeAll(&dad.object.animationSet);
    AnimationSet_FreeAll(&bf.object.animationSet);
    if (gfLoaded)
        AnimationSet_FreeAll(&gf.object.animationSet);
    gfLoaded = 0;
}

/* ------------------------------------------------------------------ */
/* Eventos                                                             */
/* ------------------------------------------------------------------ */

void PlayStateAnimations_OpponentHit(int lane) {
    dad.idleTimer = 0;
    AnimatedObject_SetAnimation(&dad.object, dad.animations.notes[lane]);
}

void PlayStateAnimations_OpponentHold(void) {
    dad.idleTimer = 0;
}

void PlayStateAnimations_BotHit(int lane) {
    bf.idleTimer = 0;
    AnimatedObject_SetAnimation(&bf.object, bf.animations.notes[lane]);
}

void PlayStateAnimations_BotHold(void) {
    bf.idleTimer = 0;
}

void PlayStateAnimations_PlayerHit(int lane) {
    if (gameOptions.hitSound)
        PlaySound(hitSFX);

    bf.idleTimer = 0;
    AnimatedObject_SetAnimation(&bf.object, bf.animations.notes[lane]);
}

void PlayStateAnimations_PlayerHold(void) {
    bf.idleTimer = 0;
}

void PlayStateAnimations_PlayerMiss(int lane) {
    static double lastMissSound = -1.0;

    bf.idleTimer = 0;

    /* chart com spam pode errar milhares de notas no mesmo quadro: 1 som a cada 40 ms */
    const double now = GetTime();
    if (now - lastMissSound >= 0.04) {
        lastMissSound = now;
        PlaySound(missSFX[GetRandomValue(0, 2)]);
    }

    AnimatedObject_SetAnimation(&bf.object, bf.missAnimations.notes[lane]);
}

/* ------------------------------------------------------------------ */
/* Frame e desenho                                                     */
/* ------------------------------------------------------------------ */

void PlayStateAnimations_Step(void) {
    if (!gfHidden)
        AnimatedObject_UpdateFrame(&gf.object);
    AnimatedObject_UpdateFrame(&dad.object);
    AnimatedObject_UpdateFrame(&bf.object);
}

void PlayStateAnimations_Draw(void) {
    if (!gfHidden)
        Render_DrawAnimatedObject(&gf.object);
    Render_DrawAnimatedObject(&dad.object);
    Render_DrawAnimatedObject(&bf.object);
}
