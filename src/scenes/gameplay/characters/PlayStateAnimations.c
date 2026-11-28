/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateAnimations.c - personagens do PlayState (oponente, BF, GF).
 */

#include "scenes/gameplay/characters/PlayStateAnimations.h"
#include "scenes/gameplay/flow/PlayStateDeath.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "scenes/gameplay/stage/PlayStateStage.h"
#include "scenes/gameplay/characters/PlayStateBeat.h"
#include "logic/notes/NoteTypes.h"
#include "GameData.h"
#include "Game.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static Girlfriend gf;
static char gfLoaded;   /* GF carregada nesta musica (descarregar so se sim) */
static char gfHidden;   /* GF nao e desenhada (chart, stage ou "empty"); pode estar carregada */
static Character bf;
static Character dad;

static Sound missSFX[3];
static Sound hitSFX;

static const char* const noGfNames[] = {
    "empty", "none", "nogf", "no-gf", "nogirlfriend", "no_gf", "__not_have_a_gf", "air", ""
};

Character* PlayStateAnimations_Dad(void) { return &dad; }
Character* PlayStateAnimations_Bf(void)  { return &bf; }
Girlfriend* PlayStateAnimations_Gf(void) { return &gf; }
Girlfriend* PlayStateAnimations_GfLoaded(void) { return gfLoaded ? &gf : NULL; }

/* ---- Estado dos eventos Psych (ResetEvents) ---- */
typedef struct {
    char on;
    double until;   /* tempo da musica em que acaba; < 0 = quando a animacao terminar */
} SpecialAnim;
static SpecialAnim special[3];   /* 0 = oponente, 1 = BF, 2 = GF */
static int gfSpeed = 1;
static int baseIdle[2];          /* idle original do oponente / BF (Alt Idle Animation volta a ele) */
static int baseGfAnims[2];

static void SaveBaseIdles(void) {
    baseIdle[0] = dad.animations.idle;
    baseIdle[1] = bf.animations.idle;
    baseGfAnims[0] = gf.anims[0];
    baseGfAnims[1] = gf.anims[1];
}

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
    SaveBaseIdles();
}

/* Volume base do hitsound; o volume global (+/-) multiplica por cima. */
#define HIT_SOUND_BASE_VOLUME 0.5f
/* Intervalo minimo entre dois hitsounds: notas em rajada nao empilham a mesma amostra. */
#define HIT_SOUND_COOLDOWN 0.10

void PlayStateAnimations_LoadSounds(void) {
    static const char* const missFiles[3] = {
        "assets/sounds/missnote1.ogg", "assets/sounds/missnote2.ogg", "assets/sounds/missnote3.ogg"
    };

    for (int i = 0; i < 3; i++) {
        missSFX[i] = LoadSound(missFiles[i]);
        SetSoundVolume(missSFX[i], 0.4f);
    }

    hitSFX = LoadSound("assets/sounds/hitosu!mania.ogg");
    SetSoundVolume(hitSFX, HIT_SOUND_BASE_VOLUME * RayGame_MusicVolumeLevel());
}

void PlayStateAnimations_SetHitVolume(float volume) {
    SetSoundVolume(hitSFX, HIT_SOUND_BASE_VOLUME * volume);
}

static void PlayHitSound(void) {
    static double lastHitSound = -1.0;

    const double now = GetTime();
    if (lastHitSound >= 0.0 && now - lastHitSound < HIT_SOUND_COOLDOWN)
        return;

    lastHitSound = now;
    PlaySound(hitSFX);
}

/* Hitsound do botplay: some quando ha spam (mais de HIT_SOUND_SPAM_MAX notas seguidas). */
#define HIT_SOUND_SPAM_MAX 4

static void PlayBotHitSound(void) {
    static double lastBotHit = -1.0;
    static int streak = 0;

    const double now = GetTime();
    if (lastBotHit >= 0.0 && now - lastBotHit < HIT_SOUND_COOLDOWN)
        streak++;
    else
        streak = 1;
    lastBotHit = now;

    if (streak > HIT_SOUND_SPAM_MAX)
        return;
    PlayHitSound();
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
/* Evento Change Character                                             */
/* ------------------------------------------------------------------ */

static int IsNoGfName(const char* name) {
    for (size_t i = 0; i < sizeof(noGfNames) / sizeof(noGfNames[0]); i++)
        if (strcmp(name, noGfNames[i]) == 0) return 1;
    return 0;
}

/* Libera o que o personagem antigo usava (textura do icone e animacoes). */
static void FreeCharacter(Character* c) {
    UnloadTexture(c->icon);
    AnimationSet_FreeAll(&c->object.animationSet);
}

/* Troca um personagem (oponente/BF) mantendo a posicao que o stage deu ao slot. */
static void SwapSide(Character* c, const char* name, CharRole role) {
    const Vector2 pos = c->object.position;
    FreeCharacter(c);
    Character_Load(c, name, role);
    c->object.position = pos;
    AnimatedObject_SetAnimation(&c->object, c->animations.idle);
    const int side = (c == &dad) ? 0 : 1;   /* personagem novo: sem sufixo nem animacao especial */
    baseIdle[side] = c->animations.idle;
    special[side].on = 0;
}

void PlayStateAnimations_ChangeCharacter(int target, const char* name) {
    if (name == NULL) name = "";

    if (target == 0) {
        SwapSide(&dad, name, CHAR_DAD);
        PlayStateHealth_SetCharacterColors(&dad, &bf);
    } else if (target == 1) {
        SwapSide(&bf, name, CHAR_BF);
        PlayStateDeath_LoadAnimations(&bf);
        PlayStateHealth_SetCharacterColors(&dad, &bf);
    } else if (target == 2) {
        const Vector2 pos = gf.object.position;
        if (gfLoaded)
            AnimationSet_FreeAll(&gf.object.animationSet);
        if (IsNoGfName(name)) {
            /* "empty"/"none": GF some da cena */
            Render_DefaultAnimated(&gf.object);
            gf.f = 0;
            gf.anims[0] = gf.anims[1] = 0;
            gfLoaded = 0;
            gfHidden = 1;
            return;
        }
        Girlfriend_Load(&gf, name);
        gf.object.position = pos;
        gfLoaded = 1;
        gfHidden = 0;
        AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);
        baseGfAnims[0] = gf.anims[0];
        baseGfAnims[1] = gf.anims[1];
        special[2].on = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Eventos Psych: Play Animation, Hey!, Alt Idle, GF Speed, alpha      */
/* ------------------------------------------------------------------ */

/* Nomes do atlas padrao para papeis comuns (personagem sem JSON). */
static const char* const animAliases[][2] = {
    {"hey", "BF HEY!!"}, {"cheer", "GF Cheer"}, {"scared", "GF FEAR "},
};

/* Papel do JSON primeiro; senao o nome direto no atlas (XML / Codename); senao o alias padrao. */
static int FindNamed(const RayAnimatedObject* obj, const CharNamedAnim* named, int count, const char* name) {
    if (name == NULL || name[0] == 0 || obj->animationSet.animationCount <= 0) return -1;
    for (int i = 0; i < count; i++)
        if (strcmp(named[i].role, name) == 0) return named[i].index;
    int idx = AnimationSet_FindAnimation(&obj->animationSet, name);
    for (size_t i = 0; idx < 0 && i < sizeof(animAliases) / sizeof(animAliases[0]); i++)
        if (strcmp(animAliases[i][0], name) == 0) idx = AnimationSet_FindAnimation(&obj->animationSet, animAliases[i][1]);
    return idx;
}

static Character* SideChar(int target) {
    return target == 0 ? &dad : target == 1 ? &bf : NULL;
}

char PlayStateAnimations_PlaySpecial(int target, const char* anim, float holdSecs) {
    if (target < 0 || target > 2) return 0;
    const double until = holdSecs > 0.0f ? PlayStateBeat_Time() + (double)holdSecs : -1.0;
    if (target == 2) {
        if (!gfLoaded || gfHidden) return 0;
        const int idx = FindNamed(&gf.object, gf.named, gf.namedCount, anim);
        if (idx < 0) return 0;
        AnimatedObject_SetAnimation(&gf.object, idx);
        special[2] = (SpecialAnim){1, until};
        return 1;
    }
    Character* c = SideChar(target);
    const int idx = FindNamed(&c->object, c->named, c->namedCount, anim);
    if (idx < 0) return 0;
    AnimatedObject_SetAnimation(&c->object, idx);
    c->idleTimer = -1.0e9f;   /* o idle automatico (PlayStateBeat) nao interrompe */
    special[target] = (SpecialAnim){1, until};
    return 1;
}

/* Fim da animacao especial: volta a dancar. Uma vez por frame (Step). */
static void UpdateSpecial(int target) {
    SpecialAnim* s = &special[target];
    if (!s->on) return;
    RayAnimatedObject* obj = target == 2 ? &gf.object : &SideChar(target)->object;
    const char done = s->until >= 0.0 ? (PlayStateBeat_Time() >= s->until) : AnimatedObject_FinishedAnimation(obj);
    if (!done) return;
    s->on = 0;
    if (target == 2) {
        AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);
        return;
    }
    Character* c = SideChar(target);
    c->idleTimer = 0.0f;
    AnimatedObject_SetAnimation(&c->object, c->animations.idle);
}

void PlayStateAnimations_SetIdleSuffix(int target, const char* suffix) {
    if (suffix == NULL) suffix = "";
    char name[48];
    if (target == 2) {
        if (!gfLoaded) return;
        if (suffix[0] == 0) {
            gf.anims[0] = baseGfAnims[0];
            gf.anims[1] = baseGfAnims[1];
            return;
        }
        snprintf(name, sizeof(name), "danceLeft%s", suffix);
        int left = FindNamed(&gf.object, gf.named, gf.namedCount, name);
        snprintf(name, sizeof(name), "danceRight%s", suffix);
        int right = FindNamed(&gf.object, gf.named, gf.namedCount, name);
        if (left < 0) {
            snprintf(name, sizeof(name), "idle%s", suffix);
            left = right = FindNamed(&gf.object, gf.named, gf.namedCount, name);
        }
        if (left < 0) return;
        gf.anims[0] = left;
        gf.anims[1] = right >= 0 ? right : left;
        return;
    }
    Character* c = SideChar(target);
    if (c == NULL) return;
    if (suffix[0] == 0) {
        c->animations.idle = baseIdle[target];
        return;
    }
    snprintf(name, sizeof(name), "idle%s", suffix);
    const int idx = FindNamed(&c->object, c->named, c->namedCount, name);
    if (idx >= 0) c->animations.idle = idx;   /* o idle automatico troca na proxima volta ao idle */
}

void PlayStateAnimations_SetGfSpeed(int beats) {
    gfSpeed = beats >= 1 ? beats : 1;
}

void PlayStateAnimations_GfBeat(int beat) {
    if (!gfLoaded || special[2].on) return;
    if (gfSpeed > 1 && beat % gfSpeed != 0) return;
    gf.f = !gf.f;
    AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);
}

void PlayStateAnimations_SetAlpha(int target, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    RayAnimatedObject* obj = target == 2 ? &gf.object : target == 0 ? &dad.object : target == 1 ? &bf.object : NULL;
    if (obj != NULL) obj->color.a = (unsigned char)(alpha * 255.0f + 0.5f);
}

void PlayStateAnimations_ResetEvents(void) {
    for (int i = 0; i < 3; i++) {
        if (special[i].on && i < 2) SideChar(i)->idleTimer = 0.0f;
        special[i].on = 0;
    }
    gfSpeed = 1;
    dad.animations.idle = baseIdle[0];
    bf.animations.idle = baseIdle[1];
    if (gfLoaded) {
        gf.anims[0] = baseGfAnims[0];
        gf.anims[1] = baseGfAnims[1];
    }
    dad.object.color.a = 255;
    bf.object.color.a = 255;
    gf.object.color.a = 255;
}
/* ------------------------------------------------------------------ */
/* Eventos                                                             */
/* ------------------------------------------------------------------ */

/* Intervalo minimo para reiniciar a mesma animacao de canto: varias camadas de nota
 * no mesmo quadro chamariam SetAnimation varias vezes e a pose ficaria presa no frame 0. */
#define SING_RETRIGGER_COOLDOWN 0.002

typedef struct {
    int anim;
    double time;
} SingTrigger;

static SingTrigger dadSing = { -1, -1.0 };
static SingTrigger bfSing = { -1, -1.0 };
static SingTrigger bfMissSing = { -1, -1.0 };
static SingTrigger gfSing = { -1, -1.0 };

/* Reinicia a animacao so se for outra, ou se a mesma ja foi disparada ha mais que o cooldown. */
static void TriggerSing(RayAnimatedObject* obj, int anim, SingTrigger* last) {
    const double now = GetTime();
    if (anim == last->anim && last->time >= 0.0 && now - last->time < SING_RETRIGGER_COOLDOWN)
        return;

    last->anim = anim;
    last->time = now;
    AnimatedObject_SetAnimation(obj, anim);
}

static const char* const singRoles[4] = {"singLEFT", "singDOWN", "singUP", "singRIGHT"};
#define GF_SING_SECS 0.45   /* GF segura a pose de canto e volta a dancar */
#define HEY_SECS 0.6f       /* heyTimer da Psych */

/* Animacao de canto (miss = singLEFTmiss...) com sufixo; sem a variante, cai na normal. */
static int SingAnim(const Character* c, int lane, const char* suffix, char miss) {
    const int base = miss ? c->missAnimations.notes[lane] : c->animations.notes[lane];
    if (suffix == NULL || suffix[0] == 0) return base;
    char name[48];
    snprintf(name, sizeof(name), "%s%s%s", singRoles[lane], miss ? "miss" : "", suffix);
    const int idx = FindNamed(&c->object, c->named, c->namedCount, name);
    return idx >= 0 ? idx : base;
}

/* Sufixo do canto: altAnim da secao (so o oponente, como a Psych) ou o do tipo. */
static const char* NoteSuffix(int side, const Note* note, const NoteTypeInfo* t) {
    if (side == 0 && note->altSection) return "-alt";
    return t->animSuffix;
}

/* GF Sing / gfSection: a GF canta no lugar; Hey! vira cheer. */
static void GfSingNote(const Note* note, const char* suffix, char hey) {
    if (!gfLoaded || gfHidden) return;
    char name[48];
    snprintf(name, sizeof(name), "%s%s", singRoles[note->id], suffix);
    int idx = FindNamed(&gf.object, gf.named, gf.namedCount, name);
    if (idx < 0 && suffix[0] != 0) idx = FindNamed(&gf.object, gf.named, gf.namedCount, singRoles[note->id]);
    if (idx >= 0) {
        TriggerSing(&gf.object, idx, &gfSing);
        special[2] = (SpecialAnim){1, PlayStateBeat_Time() + GF_SING_SECS};
    }
    if (hey) (void)PlayStateAnimations_PlaySpecial(2, "cheer", HEY_SECS);
}

/* Canto de uma nota acertada (Psych goodNoteHit / opponentNoteHit). side: 0 = oponente, 1 = BF. */
static void SingNote(int side, const Note* note) {
    const NoteTypeInfo* t = NoteTypes_Get(note->type);
    if (!t->noAnimation && note->id >= 0 && note->id < 4) {
        const char* suffix = NoteSuffix(side, note, t);
        if (t->gfNote || note->gfSection) {
            GfSingNote(note, suffix, (char)t->hey);
        } else {
            Character* c = side ? &bf : &dad;
            special[side].on = 0;   /* nota cantada interrompe Play Animation / Hey! */
            c->idleTimer = 0;
            TriggerSing(&c->object, SingAnim(c, note->id, suffix, 0), side ? &bfSing : &dadSing);
            if (t->hey) (void)PlayStateAnimations_PlaySpecial(side, "hey", HEY_SECS);
        }
    }
    if (t->hitAnimTarget >= 0)
        (void)PlayStateAnimations_PlaySpecial(t->hitAnimTarget, t->hitAnim, 0.0f);
}

/* Sustain: segura a pose de quem esta cantando. */
static void HoldNote(int side, const Note* note) {
    const NoteTypeInfo* t = NoteTypes_Get(note->type);
    if (t->noAnimation) return;
    if (t->gfNote || note->gfSection) {
        if (special[2].on && special[2].until >= 0.0) {
            const double until = PlayStateBeat_Time() + 0.1;
            if (until > special[2].until) special[2].until = until;
        }
        return;
    }
    (side ? &bf : &dad)->idleTimer = 0;
}

void PlayStateAnimations_OpponentHit(const Note* note) {
    SingNote(0, note);
}

void PlayStateAnimations_OpponentHold(const Note* note) {
    HoldNote(0, note);
}

void PlayStateAnimations_BotHit(const Note* note) {
    if (gameOptions.hitSoundBotplay)
        PlayBotHitSound();
    SingNote(1, note);
}

void PlayStateAnimations_BotHold(const Note* note) {
    HoldNote(1, note);
}

void PlayStateAnimations_PlayerHit(const Note* note) {
    if (gameOptions.hitSound)
        PlayHitSound();
    SingNote(1, note);
}

void PlayStateAnimations_PlayerHold(const Note* note) {
    HoldNote(1, note);
}

void PlayStateAnimations_PlayerMiss(int lane, const Note* note) {
    static double lastMissSound = -1.0;

    /* chart com spam pode errar milhares de notas no mesmo quadro: 1 som a cada 40 ms */
    const double now = GetTime();
    if (now - lastMissSound >= 0.04) {
        lastMissSound = now;
        PlaySound(missSFX[GetRandomValue(0, 2)]);
    }
    if (lane < 0 || lane > 3) return;

    const NoteTypeInfo* t = NoteTypes_Get(note != NULL ? note->type : NOTETYPE_NORMAL);
    if (note != NULL && t->missAnimTarget >= 0)
        (void)PlayStateAnimations_PlaySpecial(t->missAnimTarget, t->missAnim, 0.0f);
    if (t->noMissAnimation) return;
    if (note != NULL && (t->gfNote || note->gfSection)) return;   /* miss da GF: ela nao tem animacao de erro */

    special[1].on = 0;
    bf.idleTimer = 0;
    /* Hurt Note acertada: "hurt" no lugar do miss, se o BF tiver */
    if (note != NULL && t->hurtAnim && PlayStateAnimations_PlaySpecial(1, "hurt", 0.0f)) return;
    TriggerSing(&bf.object, SingAnim(&bf, lane, t->animSuffix, 1), &bfMissSing);
}

/* ------------------------------------------------------------------ */
/* Frame e desenho                                                     */
/* ------------------------------------------------------------------ */

void PlayStateAnimations_Step(void) {
    for (int i = 0; i < 3; i++)
        UpdateSpecial(i);
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
