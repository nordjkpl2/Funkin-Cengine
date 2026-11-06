/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl outd day30 of10 2026 */
/*
 * PlayState.c - coordenador da gameplay.
 * Cada parte pesada vive em um modulo gameplay/PlayState*.c; aqui ficam o estado
 * da cena, os receptores, o fluxo (intro, morte, pausa, fim da musica) e a ligacao
 * entre os modulos por callbacks.
 */
#include "scenes/AllScenes.h"
#include "scenes/Character.h"
#include "scenes/Note.h"
#include "scenes/Song.h"
#include "scenes/gameplay/PlayStateStage.h"
#include "scenes/gameplay/PlayStateHealth.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "scenes/gameplay/PlayStateText.h"
#include "scenes/gameplay/PlayStateNoteSpawn.h"
#include "scenes/gameplay/PlayStatePlayerInput.h"
#include "scenes/gameplay/PlayStateCamera.h"
#include "Log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define LANES 4
#define LANE_SPACING 110.0f

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

static float smooth(float a, float b, float c) {
    return Lerp(a, b, c * c * c * c);
}

// cena
static char songDir[256];
static char returnToEditor, returnToWeek, diffSuf[16], overrideP2[32];
static char restarted;
static float lastAppliedMusicVolume = -1.0f;

// musica e batida
static Song song;
static BeatManager songBeat;
static int currentSteps;
static int currentSection;

// intro e popup de rating
static int introHit;
static float introTime, introStep;
static RayAnimationHandler uiAssets;
static RayAnimatedObject introText;
static Sound introSounds[4];
static int ratingAnim[4]; // indexado por PlayerRating
static RayAnimatedObject popupObj;
static float popupTimer;
static float popupVelocity;

// personagens e cenario
static Girlfriend gf;
static char gfHidden;
static Character bf;
static Character dad;
static PlayStateStage stage;

// receptores
static StrumNote dadStrumline[LANES];
static StrumNote bfStrumline[LANES];
static Vector2 dadStrumIdleSizes[LANES];
static Vector2 bfStrumIdleSizes[LANES];
static float bfMiddleX = 475.0f; // middlescroll: X do 1o receptor do BF (centralizado)

// sons
static Sound missSFX[3];
static Sound hitSFX;

// morte
static int deathAnimations[3];
static float deadTime;
static char justDied;
static char retryin;
static Music deathMusic, deathMusicEnd;
static Sound deathSounds;

// pausa
static char isPaused;
static int currentOption;
static float optionOffset;
static float pauseTimer;

/* ------------------------------------------------------------------ */
/* Receptores (game_options_scroll: 0 = up, 1 = down, 2 = middle)      */
/* ------------------------------------------------------------------ */

static float StrumY(void) {
    if (game_options_scroll == 2) return 20.0f;
    if (game_options_scroll == 1) return 600.0f;
    return 50.0f;
}

static float StrumX(char mustHit, int lane) {
    if (game_options_scroll != 2)
        return (mustHit ? 700.0f : 50.0f) + LANE_SPACING * lane;

    return mustHit ? bfMiddleX + LANE_SPACING * lane : 50.0f + 55.0f * lane;
}

static float NoteVelocity(void) {
    return game_options_scroll == 1 ? 450.0f : -450.0f;
}

static void Strumlines_Load(void) {
    const char middle = game_options_scroll == 2;

    for (int i = 0; i < LANES; i++) {
        StrumNote_Load(dadStrumline + i, i);
        StrumNote_Load(bfStrumline + i, i);

        dadStrumline[i].object.position.y = StrumY();
        bfStrumline[i].object.position.y = StrumY();

        // Middlescroll: oponente menor; BF na escala normal das notas.
        if (middle)
            dadStrumline[i].object.scaleX = dadStrumline[i].object.scaleY = 0.5f;
    }

    if (middle) {
        RayAnimatedObject idle = bfStrumline[0].object;
        AnimatedObject_SetAnimation(&idle, StrumNote_IdleAnimation(0));
        bfMiddleX = SCREEN_W / 2 - (LANE_SPACING * (LANES - 1) + AnimatedObject_Sizes(&idle).x) * 0.5f;
    }

    for (int i = 0; i < LANES; i++) {
        dadStrumline[i].object.position.x = StrumX(0, i);
        bfStrumline[i].object.position.x = StrumX(1, i);
    }

    // Cacheia o tamanho idle uma vez: nao mede/refaz a animacao a cada frame.
    for (int i = 0; i < LANES; i++) {
        RayAnimatedObject dadIdle = dadStrumline[i].object;
        RayAnimatedObject bfIdle = bfStrumline[i].object;

        AnimatedObject_SetAnimation(&dadIdle, StrumNote_IdleAnimation(i));
        AnimatedObject_SetAnimation(&bfIdle, StrumNote_IdleAnimation(i));

        dadStrumIdleSizes[i] = AnimatedObject_Sizes(&dadIdle);
        bfStrumIdleSizes[i] = AnimatedObject_Sizes(&bfIdle);
    }
}

/*
 * Desenha o receptor mantendo o centro visual da animacao idle como ancora.
 * Animacoes maiores (confirm, por causa do brilho) fariam a seta "saltar";
 * o tamanho idle e medido uma vez no load. A posicao logica da coluna e
 * restaurada depois do desenho, entao notas e sustains usam a coordenada fixa.
 */
static void RenderStrumFixedCenter(StrumNote* strum, Vector2 idleSize) {
    Vector2 currentSize = AnimatedObject_Sizes(&strum->object);
    Vector2 originalPosition = strum->object.position;

    strum->object.position.x += (idleSize.x - currentSize.x) * 0.5f;
    strum->object.position.y += (idleSize.y - currentSize.y) * 0.5f;

    Render_DrawAnimatedObject(&strum->object);

    strum->object.position = originalPosition;
}

static void Strumlines_Draw(void) {
    for (int i = 0; i < LANES; i++) {
        AnimatedObject_UpdateFrame(&dadStrumline[i].object);
        RenderStrumFixedCenter(dadStrumline + i, dadStrumIdleSizes[i]);

        AnimatedObject_UpdateFrame(&bfStrumline[i].object);
        RenderStrumFixedCenter(bfStrumline + i, bfStrumIdleSizes[i]);
    }

    // Botplay: volta o BF ao idle quando a animacao de confirm termina.
    if (game_options_botplay) {
        for (int i = 0; i < LANES; i++) {
            if (AnimatedObject_FinishedAnimation(&bfStrumline[i].object))
                AnimatedObject_SetAnimation(&bfStrumline[i].object, StrumNote_IdleAnimation(i));
        }
    }
}

// Animacao dos receptores do BF a partir das teclas deste frame.
static void Strumlines_ApplyInput(const PlayStatePlayerKeys* keys) {
    for (int i = 0; i < LANES; i++) {
        RayAnimatedObject* strum = &bfStrumline[i].object;

        if (keys->pressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_ConfirmAnimation(i));
        else if (!keys->keyDown[i]) {
            const int idle = StrumNote_IdleAnimation(i);
            if (strum->currentAnim.animationIndex != idle)
                AnimatedObject_SetAnimation(strum, idle);
        }
        else if (keys->keyPressed[i])
            AnimatedObject_SetAnimation(strum, StrumNote_PressAnimation(i));
    }
}

/* ------------------------------------------------------------------ */
/* Eventos (chamados pelos modulos NoteSpawn e PlayerInput)            */
/* ------------------------------------------------------------------ */

static void Event_OpponentHit(Note* note) {
    dad.idleTimer = 0;
    AnimatedObject_SetAnimation(&dad.object, dad.animations.notes[note->id]);
    AnimatedObject_SetAnimation(&dadStrumline[note->id].object, StrumNote_ConfirmAnimation(note->id));
}

static void Event_BotHit(Note* note) {
    PlayStatePlayerInput_AddScore(350); // sick fixo no botplay
    bf.idleTimer = 0;
    AnimatedObject_SetAnimation(&bf.object, bf.animations.notes[note->id]);
    AnimatedObject_SetAnimation(&bfStrumline[note->id].object, StrumNote_ConfirmAnimation(note->id));
}

static void Event_OpponentHold(void) { dad.idleTimer = 0; }
static void Event_BotHold(void) { bf.idleTimer = 0; }

static void Event_PlayerHold(void) {
    PlayStateHealth_Add(0.04f * RayGame_DeltaTime());
    bf.idleTimer = 0;
}

static void Event_PlayerHit(Note* note, PlayerRating rating) {
    AnimatedObject_SetAnimation(&popupObj, ratingAnim[rating]);

    Vector2 size = AnimatedObject_Sizes(&popupObj);
    popupObj.position.x = PlayStateHealth_CenterX(size.x);
    popupObj.position.y = PlayStateHealth_BarY() + GetRandomValue(-1000, 1000) / 200.0f;
    popupVelocity = -150;
    popupTimer = 1.0f;

    if (game_options_hitSound)
        PlaySound(hitSFX);

    bf.idleTimer = 0;
    AnimatedObject_SetAnimation(&bf.object, bf.animations.notes[note->id]);
}

static void Event_PlayerMiss(int lane) {
    bf.idleTimer = 0;
    PlaySound(missSFX[GetRandomValue(0, 2)]);
    AnimatedObject_SetAnimation(&bf.object, bf.missAnimations.notes[lane]);
}

static void SetupEvents(void) {
    static const PlayStateNoteEvents noteEvents = {
        .opponentHit = Event_OpponentHit,
        .botHit = Event_BotHit,
        .opponentHold = Event_OpponentHold,
        .botHold = Event_BotHold,
        .playerHold = Event_PlayerHold,
        .miss = PlayStatePlayerInput_MissNote,
    };
    static const PlayStatePlayerInputEvents inputEvents = {
        .hit = Event_PlayerHit,
        .miss = Event_PlayerMiss,
    };

    PlayStateNoteSpawn_SetEvents(&noteEvents);
    PlayStatePlayerInput_SetEvents(&inputEvents);
}

/* ------------------------------------------------------------------ */
/* Volume, batida e secoes                                             */
/* ------------------------------------------------------------------ */

// Propaga o volume global (Game.c) para todas as faixas; so age quando o valor muda.
static void ApplyMusicVolume(void) {
    const float volume = RayGame_MusicVolumeLevel();

    if (volume == lastAppliedMusicVolume)
        return;

    lastAppliedMusicVolume = volume;

    PlayStateAudio_SetVolume(volume);

    if (deathMusic.stream.buffer != NULL)
        SetMusicVolume(deathMusic, volume);

    if (deathMusicEnd.stream.buffer != NULL)
        SetMusicVolume(deathMusicEnd, volume);
}

static void UpdateSection(void) {
    if (currentSection > (int)song.sectionCount - 1)
        return;

    Section* section = song.sections + currentSection;
    PlayStateCamera_Follow(section->mustHit ? &bf : &dad);
    songBeat.bpm = section->bpm;
}

static void StepHit(void) {
    if (currentSection > (int)song.sectionCount - 1)
        return;

    PlayStateAudio_SyncVoices();

    Section* section = song.sections + currentSection;
    currentSteps++;
    if (currentSteps >= section->len) {
        currentSteps = 0;
        currentSection++;
        UpdateSection();
    }
}

// Volta a um personagem para idle no beat, se ele nao estiver cantando.
static void BeatIdle(Character* character) {
    if (character->object.currentAnim.animationIndex != character->animations.idle)
        return;

    if (!character->idled || AnimatedObject_FinishedAnimation(&character->object))
        AnimatedObject_SetAnimation(&character->object, character->animations.idle);

    character->idled = 0;
}

static void BeatHit(void) {
    gf.f = !gf.f;
    AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);

    if (game_options_bopIcons)
        PlayStateHealth_Bop();

    if (songBeat.beat % 4 == 0)
        PlayStateCamera_Bump(0.03f * game_options_zoomFactorUI, 0.015f * game_options_zoomFactorGAME);

    if (songBeat.beat % 2 == 0) {
        BeatIdle(&bf);
        BeatIdle(&dad);
    }
}

// Zera a batida e agenda a contagem regressiva (5 passos antes da musica).
static void StartBeat(void) {
    BeatManager_New(&songBeat);
    songBeat.music = PlayStateAudio_Inst();
    songBeat.bpm = song.sections[0].bpm;

    introHit = 0;
    introTime = -0.5f;
    introStep = 60.0f / songBeat.bpm;
    songBeat.time = introTime + -5 * introStep;
}

/* ------------------------------------------------------------------ */
/* Create                                                              */
/* ------------------------------------------------------------------ */

static void ResetState(void) {
    currentSteps = 0;
    currentSection = 0;
    lastAppliedMusicVolume = -1.0f;

    PlayStatePlayerInput_Reset();
    PlayStateText_Reset();
    PlayStateAudio_Reset();
    PlayStateHealth_Reset();
    PlayStateCamera_Reset(&dad);

    popupTimer = 0;
    isPaused = 0;
    currentOption = 0;
    optionOffset = 0;

    deadTime = 0;
    justDied = 1;
    retryin = 0;
}

// Restart: reaproveita tudo que ja esta carregado.
static void Restart(void) {
    AnimatedObject_SetAnimation(&dad.object, dad.animations.idle);
    AnimatedObject_SetAnimation(&bf.object, bf.animations.idle);
    AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);

    bf.object.color.a = 255;
    PlayStateNoteSpawn_Reset();

    PlayStateAudio_Load(songDir);
    PlayStateAudio_StartPaused();

    StartBeat();

    PlayStateCamera_SetPosition(dad.object.position.x + SCREEN_W / 2, dad.object.position.y + SCREEN_H / 2);
    UpdateSection();

    introText.currentAnim.isValid = 0;
    restarted = 0;
}

static void LoadSong(void) {
    if (diffSuf[0] != 0)
        Song_LoadSongFromDirDiff(&song, songDir, diffSuf);
    else
        Song_LoadSongFromDir(&song, songDir);

    if (strcmp(diffSuf, "easy") == 0)
        Note_SetWindow(0.240f);
    else if (strcmp(diffSuf, "hard") == 0)
        Note_SetWindow(0.180f);
    else
        Note_SetWindow(0.200f);
}

static void LoadCharacters(void) {
    // gfVersion do chart: "empty"/"none"... = sem GF (carrega a padrao so por seguranca, sem desenhar).
    static const char* const noGf[] = {"empty", "none", "nogf", "no-gf", "nogirlfriend", "no_gf", ""};
    gfHidden = 0;
    for (size_t i = 0; i < sizeof(noGf) / sizeof(noGf[0]); i++)
        if (strcmp(song.gfVersion, noGf[i]) == 0) gfHidden = 1;

    Character_SetSongDir(songDir); // personagens do mod da musica
    Girlfriend_Load(&gf, gfHidden ? NULL : song.gfVersion);
    Character_Load(&dad, overrideP2[0] != 0 ? overrideP2 : song.player2, CHAR_DAD);
    Character_Load(&bf, song.player1, CHAR_BF);
    Character_LoadDeathAnimations(&bf, deathAnimations);
    PlayStateHealth_SetCharacterColors(&dad, &bf);
}

static void LoadSounds(void) {
    static const char* const missFiles[3] = {
        "assets/sounds/missnote1.ogg", "assets/sounds/missnote2.ogg", "assets/sounds/missnote3.ogg"
    };
    static const char* const introFiles[4] = {
        "assets/sounds/intro3.ogg", "assets/sounds/intro2.ogg", "assets/sounds/intro1.ogg", "assets/sounds/introGo.ogg"
    };

    for (int i = 0; i < 3; i++) {
        missSFX[i] = LoadSound(missFiles[i]);
        SetSoundVolume(missSFX[i], 0.4f);
    }

    hitSFX = LoadSound("assets/sounds/hitosu!mania.ogg");
    SetSoundVolume(hitSFX, 0.5f);

    for (int i = 0; i < 4; i++) {
        introSounds[i] = LoadSound(introFiles[i]);
        SetSoundVolume(introSounds[i], 0.7f);
    }

    deathSounds = LoadSound("assets/sounds/fnf_loss_sfx.ogg");
    deathMusic = LoadMusicStream("assets/music/gameOver.ogg");
    deathMusic.looping = 1;
    deathMusicEnd = LoadMusicStream("assets/music/gameOverEnd.ogg");
    deathMusicEnd.looping = 0;
}

static void LoadUiAssets(void) {
    uiAssets = AnimationSet_LoadAnimations("assets/images/ui.animset", Render_LoadTexture("assets/images/ui.png"));

    ratingAnim[PLAYER_RATING_SICK] = AnimationSet_FindAnimation(&uiAssets, "sick");
    ratingAnim[PLAYER_RATING_GOOD] = AnimationSet_FindAnimation(&uiAssets, "good");
    ratingAnim[PLAYER_RATING_BAD] = AnimationSet_FindAnimation(&uiAssets, "bad");
    ratingAnim[PLAYER_RATING_SHIT] = AnimationSet_FindAnimation(&uiAssets, "shit");

    Render_DefaultAnimated(&introText);
    introText.animationSet = uiAssets;
    introText.scaleX = introText.scaleY = 0.7f;

    Render_DefaultAnimated(&popupObj);
    popupObj.animationSet = uiAssets;
    popupObj.scaleX = popupObj.scaleY = 0.6f;
}

static void PlayState_Create([[maybe_unused]] RayScene* scene) {
    ResetState();

    if (restarted) {
        Restart();
        return;
    }

    PlayStateHealth_Init(game_options_scroll == 1);
    PlayStateText_Init(songDir);

    Strumlines_Load();
    PlayStateNoteSpawn_Init();
    SetupEvents();

    LoadSong();
    LoadCharacters();
    LoadSounds();

    PlayStateStage_Load(&stage, song.stage, songDir, &gf, &dad, &bf);

    PlayStateAudio_Load(songDir);
    PlayStateNoteSpawn_Load(&song);
    PlayStateAudio_StartPaused();

    StartBeat();
    PlayStateCamera_SetPosition(dad.object.position.x, dad.object.position.y);
    UpdateSection();

    LoadUiAssets();

    PlayStateCamera_SetGameZoom(stage.defaultCameraZoom);

    // Aquece os streams de audio antes do primeiro frame.
    ClearBackground(BLACK);
    for (int i = 0; i < 10; i++)
        PlayStateAudio_UpdateStreams();
}

/* ------------------------------------------------------------------ */
/* Saida de cena                                                       */
/* ------------------------------------------------------------------ */

// Volta para a tela de origem: semana > editor (se permitido) > freeplay.
static void LeaveGameplay(char allowEditor) {
    if (returnToWeek) {
        WeekRun_Stop();
        StoryMode_SetScene();
    } else if (allowEditor && returnToEditor) {
        ChartEditor_SetSongDir(songDir);
        ChartEditor_SetScene();
    } else {
        Freeplay_SetScene();
    }
}

// Fim da musica na semana: soma o score e segue para a proxima ou encerra.
static void FinishWeekSong(void) {
    WeekRun_AddScore(PlayStatePlayerInput_Score());

    if (WeekRun_Advance()) {
        PlayState_SetSongDir(WeekRun_Dir());
        PlayState_SetReturnWeek(1);
        PlayState_SetCharsOverride(WeekRun_Char());
        PlayState_SetDiff(WeekRun_DiffSuffix());
        PlayState_SetScene();
    } else {
        WeekRun_Finish();
        StoryMode_SetScene();
    }
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */

static void UpdateDeath(float dt) {
    PlayStateAudio_Pause();

    if (justDied) {
        AnimatedObject_SetAnimation(&bf.object, deathAnimations[0]);
        deadTime = -AnimationSet_AnimationLength(&bf.object.animationSet, deathAnimations[0]);
        justDied = 0;
        retryin = 0;
        PlaySound(deathSounds);
    }

    PlayStateCamera_Follow(&bf);

    if (retryin) {
        UpdateMusicStream(deathMusicEnd);
        bf.object.color.a = smooth(255.0f, 0, deadTime / 2);

        if (deadTime >= 2.0f) {
            StopMusicStream(deathMusicEnd);
            restarted = 1;
            PlayState_SetScene();
            return;
        }
    }
    else if (deadTime > 0) {
        if (bf.object.currentAnim.animationIndex != deathAnimations[1]) {
            PlayMusicStream(deathMusic);
            AnimatedObject_SetAnimation(&bf.object, deathAnimations[1]);
        }
        else {
            UpdateMusicStream(deathMusic);

            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                retryin = 1;
                deadTime = 0;
                AnimatedObject_SetAnimation(&bf.object, deathAnimations[2]);
                StopMusicStream(deathMusic);
                PlayMusicStream(deathMusicEnd);
            }
            else if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
                StopMusicStream(deathMusic);
                LeaveGameplay(0);
                return;
            }
        }
    }

    deadTime += dt;
}

static void UpdateIntro(float dt) {
    static const char* const countdownAnims[3] = {"ready", "set", "go"};

    introTime += dt;
    songBeat.time += dt;

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

        gf.f = !gf.f;
        AnimatedObject_SetAnimation(&gf.object, gf.anims[gf.f]);

        introHit++;
        introTime -= introStep;
    }

    introText.color.a = smooth(255.0f, 0, introTime / introStep);
}

// Retorna 1 se a cena mudou (fim da musica).
static char UpdateSong(void) {
    PlayStateAudio_Update();

    BeatManager_Update(&songBeat, StepHit, BeatHit);

    if (!PlayStateAudio_IsPlaying()) {
        RayGame_ResetMusic();
        RayGame_ToggleMusic(1);

        if (returnToWeek)
            FinishWeekSong();
        else
            LeaveGameplay(1);
        return 1;
    }

    PlayStatePlayerKeys keys;
    if (PlayStatePlayerInput_Update(songBeat.time, &keys))
        Strumlines_ApplyInput(&keys);

    return 0;
}

static void PlayState_Update([[maybe_unused]] RayScene* scene) {
    const float dt = RayGame_DeltaTime();

    ApplyMusicVolume();

    if (!IsWindowFocused()) {
        PlayStateAudio_Pause();
        return;
    }

    if (isPaused) {
        pauseTimer += dt;
        return;
    }

    // No botplay, vida nunca e menor que 0
    if (!game_options_botplay && PlayStateHealth_IsDead()) {
        UpdateDeath(dt);
    }
    else if (songBeat.time < 0) {
        UpdateIntro(dt);
    }
    else if (UpdateSong()) {
        return;
    }

    if (!PlayStateHealth_IsDead() && IsKeyPressed(KEY_ENTER)) {
        pauseTimer = -0.1f;
        isPaused = 1;
        PlayStateAudio_Pause();
    }

    PlayStateHealth_Update(dt);
    PlayStateCamera_Update(dt, stage.defaultCameraZoom);
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void PauseMenu_Update(void) {
    static const char* const options[3] = {"Resume", "Restart", "Exit to Menu"};
    const float dt = RayGame_DeltaTime();

    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color) {0, 0, 0, 100});

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        currentOption++;
        if (currentOption >= 3)
            currentOption = 0;
    }
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        currentOption--;
        if (currentOption < 0)
            currentOption = 2;
    }
    else if (pauseTimer > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        if (currentOption == 0)
            isPaused = 0;
        else if (currentOption == 1) {
            restarted = 1;
            PlayState_SetScene();
            return;
        }
        else {
            RayGame_ResetMusic();
            RayGame_ToggleMusic(1);
            LeaveGameplay(1);
            return;
        }
    }

    optionOffset = Lerp(optionOffset, currentOption, dt * 10.0f);

    Color color = WHITE;
    for (int i = 0; i < 3; i++) {
        const int fontSize = 85;
        Vector2 pos = {150.0f + fabsf(i - optionOffset) * -30, SCREEN_H / 2 + (i - optionOffset) * (15 + fontSize)};
        color.a = currentOption == i ? 255 : 150;
        DrawTextEx(mainFont, options[i], pos, fontSize, 5, color);
    }
}

// sing_duration em passos (padrao 4 = 1 beat).
static void UpdateCharacterIdle(Character* character, float beatSeconds, float dt) {
    const float singTime = beatSeconds * (character->singDuration / 4.0f);

    character->idleTimer += dt;
    if (character->object.currentAnim.animationIndex != character->animations.idle && character->idleTimer > singTime) {
        character->idled = 1;
        AnimatedObject_SetAnimation(&character->object, character->animations.idle);
    }
}

static void DrawWorld(float dt) {
    PlayStateCamera_BeginGame();

    PlayStateStage_DrawBack(&stage, isPaused);

    if (!isPaused) {
        const float beatSeconds = 60.0f / songBeat.bpm;
        UpdateCharacterIdle(&dad, beatSeconds, dt);
        UpdateCharacterIdle(&bf, beatSeconds, dt);

        if (!gfHidden) AnimatedObject_UpdateFrame(&gf.object);
        AnimatedObject_UpdateFrame(&dad.object);
        AnimatedObject_UpdateFrame(&bf.object);
    }

    if (!gfHidden) Render_DrawAnimatedObject(&gf.object);
    Render_DrawAnimatedObject(&dad.object);
    Render_DrawAnimatedObject(&bf.object);

    PlayStateStage_DrawFront(&stage, isPaused);

    Render_StopCamera();
}

static void DrawHud(float dt) {
    Strumlines_Draw();

    const PlayStateNoteView view = {
        .time = songBeat.time,
        .songSpeed = song.speed,
        .scrollVelocity = NoteVelocity(),
        .paused = isPaused,
        .dadStrums = dadStrumline,
        .bfStrums = bfStrumline,
    };
    PlayStateNoteSpawn_Draw(&view);

    PlayStateHealth_Draw(dad.icon, bf.icon);
    PlayStateText_Draw(PlayStatePlayerInput_Score(), PlayStatePlayerInput_Misses(), songBeat.time, game_options_showScore);

    if (songBeat.time < 0)
        Render_DrawAnimatedObject(&introText);

    // No botplay nao cria popup para nao matar o FPS em spam notes
    if (!game_options_botplay && popupTimer > 0) {
        popupVelocity += 500 * dt;
        popupObj.position.y += popupVelocity * dt;
        popupObj.color.a = smooth(255.0f, 0, 1 - popupTimer);

        Render_DrawAnimatedObject(&popupObj);
        popupTimer -= dt;
    }

    // Marca d'agua fixa: sem movimento, escala pulsante ou fade.
    if (game_options_botplay)
        PlayStateText_DrawBotplayMark();
}

static void PlayState_Draw([[maybe_unused]] RayScene* scene) {
    if (!game_options_botplay && PlayStateHealth_IsDead()) {
        ClearBackground(BLACK);

        PlayStateCamera_BeginGame();
        AnimatedObject_UpdateFrame(&bf.object);
        Render_DrawAnimatedObject(&bf.object);
        Render_StopCamera();
        return;
    }

    const float dt = RayGame_DeltaTime();

    ClearBackground(DARKGRAY);

    DrawWorld(dt);

    PlayStateCamera_BeginHUD();
    DrawHud(dt);

    if (isPaused)
        PauseMenu_Update();
    Render_StopCamera();
}

/* ------------------------------------------------------------------ */
/* API publica (declarada em AllScenes.h)                              */
/* ------------------------------------------------------------------ */

void PlayState_SetSongDir(const char* dir) {
    PlayState_SetSongDirKeep(dir);
    returnToEditor = 0;
    returnToWeek = 0;
    diffSuf[0] = 0;
    overrideP2[0] = 0;
}

void PlayState_SetSongDirKeep(const char* dir) {
    strncpy(songDir, dir, sizeof(songDir) - 1);
    songDir[sizeof(songDir) - 1] = 0;
}

void PlayState_SetReturnEditor(char on) {
    returnToEditor = on;
}

void PlayState_SetReturnWeek(char on) {
    returnToWeek = on;
}

void PlayState_SetDiff(const char* diff) {
    if (diff != NULL)
        strncpy(diffSuf, diff, sizeof(diffSuf) - 1);
    else
        diffSuf[0] = 0;
    diffSuf[sizeof(diffSuf) - 1] = 0;
}

void PlayState_SetCharsOverride(const char* p2) {
    if (p2 != NULL)
        strncpy(overrideP2, p2, sizeof(overrideP2) - 1);
    else
        overrideP2[0] = 0;
    overrideP2[sizeof(overrideP2) - 1] = 0;
}

int PlayState_GetScore(void) {
    return PlayStatePlayerInput_Score();
}

/* ------------------------------------------------------------------ */
/* Destroy e registro da cena                                          */
/* ------------------------------------------------------------------ */

static void PlayState_Destroy([[maybe_unused]] RayScene* scene) {
    PlayStateAudio_Unload();
    if (restarted) return; // restart reaproveita tudo

    for (int i = 0; i < 3; i++)
        UnloadSound(missSFX[i]);

    UnloadSound(hitSFX);

    UnloadSound(deathSounds);
    UnloadMusicStream(deathMusic);
    UnloadMusicStream(deathMusicEnd);

    for (int i = 0; i < 4; i++)
        UnloadSound(introSounds[i]);

    PlayStateNoteSpawn_Unload();
    Song_Free(&song);

    UnloadTexture(dad.icon);
    UnloadTexture(bf.icon);

    AnimationSet_FreeAll(&uiAssets);
    AnimationSet_FreeAll(&dad.object.animationSet);
    AnimationSet_FreeAll(&bf.object.animationSet);
    AnimationSet_FreeAll(&gf.object.animationSet);

    PlayStateStage_Unload(&stage);
    PlayStateText_Unload();
}

static RayScene scene;
Scene_MakeSceneCode(
    scene,
    PlayState_SetScene,
    PlayState_Create,
    PlayState_Update,
    PlayState_Draw,
    PlayState_Destroy
)
