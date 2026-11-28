/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateBeat.c - relogio da musica do PlayState.
 */

#include "scenes/gameplay/characters/PlayStateBeat.h"
#include <math.h>
#include "scenes/gameplay/characters/PlayStateAnimations.h"
#include "scenes/gameplay/audio/PlayStateAudio.h"
#include "scenes/gameplay/visual/PlayStateCamera.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "scenes/gameplay/flow/PlayStateIntro.h"
#include "logic/beatmanager/BeatManager.h"
#include "GameData.h"

static const Song* beatSong;   /* musica da cena (ponteiro para o Song do PlayState) */
static BeatManager songBeat;
static int currentSteps;
static int currentSection;
static float beatSeconds;      /* 60 / bpm: recalculado so quando o bpm muda, nao a cada frame */

static void SetBpm(float bpm) {
    songBeat.bpm = bpm;
    beatSeconds = 60.0f / bpm;
}

/* Troca a secao atual: a camera segue quem canta e o bpm passa a ser o da secao. */
static void UpdateSection(void) {
    if (currentSection >= beatSong->sectionCount)
        return;

    const Section* section = beatSong->sections + currentSection;
    PlayStateCamera_Follow(section->mustHit ? PlayStateAnimations_Bf() : PlayStateAnimations_Dad());
    SetBpm(section->bpm);
}

static void StepHit(void) {
    if (currentSection >= beatSong->sectionCount)
        return;

    PlayStateAudio_SyncVoices();

    const Section* section = beatSong->sections + currentSection;
    if (++currentSteps >= section->len) {
        currentSteps = 0;
        currentSection++;
        UpdateSection();
    }
}

/* Volta a um personagem para idle no beat, se ele nao estiver cantando. */
static void BeatIdle(Character* character) {
    if (character->object.currentAnim.animationIndex != character->animations.idle)
        return;

    if (!character->idled || AnimatedObject_FinishedAnimation(&character->object))
        AnimatedObject_SetAnimation(&character->object, character->animations.idle);

    character->idled = 0;
}

static void BeatHit(void) {
    Girlfriend* gf = PlayStateAnimations_GfLoaded();
    if (gf) {
        gf->f = !gf->f;
        AnimatedObject_SetAnimation(&gf->object, gf->anims[gf->f]);
    }

    if (gameOptions.bopIcons)
        PlayStateHealth_Bop();

    /* beat nunca e negativo: "& 3" e "& 1" equivalem a "% 4" e "% 2" sem divisao */
    const int beat = songBeat.beat;
    if ((beat & 3) == 0)
        PlayStateCamera_Bump(0.03f * gameOptions.zoomFactorUI, 0.015f * gameOptions.zoomFactorGAME);

    if ((beat & 1) == 0) {
        BeatIdle(PlayStateAnimations_Bf());
        BeatIdle(PlayStateAnimations_Dad());
    }
}

/* sing_duration em passos (padrao 4 = 1 beat). */
static void UpdateCharacterIdle(Character* character, float beatSecs, float dt) {
    const float singTime = beatSecs * (character->singDuration * 0.25f);

    character->idleTimer += dt;
    if (character->object.currentAnim.animationIndex != character->animations.idle && character->idleTimer > singTime) {
        character->idled = 1;
        AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    }
}

void PlayStateBeat_Start(const Song* song) {
    beatSong = song;
    currentSteps = 0;
    currentSection = 0;

    BeatManager_New(&songBeat);
    songBeat.music = PlayStateAudio_Inst();
    SetBpm(song->sections[0].bpm);

    songBeat.time = PlayStateIntro_Start(songBeat.bpm);
    UpdateSection();
}

void PlayStateBeat_Update(void) {
    songBeat.latency = PlayStateAudio_OutputDelay(); // 0 em 1x; com stretch o som sai ~0.1-0.2 s depois
    BeatManager_Update(&songBeat, StepHit, BeatHit);
}

/* Duracao de um step (1/4 de batida) para um bpm. Protegido contra bpm invalido. */
static double StepSeconds(float bpm) {
    return 15.0 / (double)(bpm >= 1.0f ? bpm : 1.0f);
}

/* Reposiciona o relogio em t (segundos). Percorre as secoes para achar secao, step e bpm
   corretos, e atualiza camera e bpm como se a musica tivesse chegado ali. Nao toca em notas. */
void PlayStateBeat_SeekTo(double t) {
    if (beatSong == NULL || !isfinite(t) || t < 0.0)
        return;

    double sectionStart = 0.0;   /* tempo em que a secao atual comeca */
    int globalStep = 0;          /* steps completos antes da secao atual */
    int section = 0;
    int localSteps = 0;
    double stepSecs = StepSeconds(beatSong->bpm);

    for (; section < beatSong->sectionCount; section++) {
        const Section* s = beatSong->sections + section;
        stepSecs = StepSeconds(s->bpm);
        const double span = (double)s->len * stepSecs;

        if (t < sectionStart + span) {
            localSteps = (int)floor((t - sectionStart) / stepSecs);
            break;
        }
        sectionStart += span;
        globalStep += s->len;
    }

    // Depois da ultima secao: fica no fim, sem novos steps.
    if (section >= beatSong->sectionCount)
        localSteps = 0;

    currentSection = section;
    currentSteps = localSteps;

    const int step = globalStep + localSteps;
    const double btime = sectionStart + (double)localSteps * stepSecs;
    BeatManager_SeekTo(&songBeat, t, btime, step);

    UpdateSection();   // ajusta camera e bpm da secao alvo (no-op se passou do fim)
}

char PlayStateBeat_SingerIsBf(void) {
    if (beatSong == NULL || currentSection >= beatSong->sectionCount)
        return 1;
    return beatSong->sections[currentSection].mustHit;
}

double PlayStateBeat_Time(void) {
    return songBeat.time;
}

char PlayStateBeat_InCountdown(void) {
    return songBeat.time < 0;
}

void PlayStateBeat_Countdown(float deltaTime) {
    songBeat.time += deltaTime;
    PlayStateIntro_Update(deltaTime, PlayStateAnimations_GfLoaded());
}

void PlayStateBeat_UpdateIdle(float deltaTime) {
    UpdateCharacterIdle(PlayStateAnimations_Dad(), beatSeconds, deltaTime);
    UpdateCharacterIdle(PlayStateAnimations_Bf(), beatSeconds, deltaTime);
}
