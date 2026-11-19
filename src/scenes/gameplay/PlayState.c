/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl outd day30 of10 2026 */
/*
 * PlayState.c - coordenador da gameplay.
 * Cada parte pesada vive em um modulo gameplay/PlayState*.c. Aqui ficam o estado da cena,
 * a ordem de carregamento, o fluxo (saida, fim da musica) e a ligacao entre os modulos
 * por callbacks. Batida, popup de rating e personagens tem modulo proprio.
 */
#include "scenes/AllScenes.h"
#include "logic/notes/Note.h"
#include "data/Song.h"
#include "scenes/gameplay/PlayStateStage.h"
#include "scenes/gameplay/PlayStateHealth.h"
#include "scenes/gameplay/PlayStateAudio.h"
#include "scenes/gameplay/PlayStateText.h"
#include "scenes/gameplay/PlayStateNoteSpawn.h"
#include "scenes/gameplay/PlayStatePlayerInput.h"
#include "scenes/gameplay/PlayStateCamera.h"
#include "scenes/gameplay/PlayStateStrums.h"
#include "scenes/gameplay/PlayStatePause.h"
#include "scenes/gameplay/PlayStateDeath.h"
#include "scenes/gameplay/PlayStateIntro.h"
#include "scenes/gameplay/PlayStateBeat.h"
#include "scenes/gameplay/PlayStateAnimations.h"
#include "scenes/gameplay/PlayStateRatingPopups.h"
#include "Log.h"
#include "rendering/cpu/ImagePrefetch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Game.h"
#include "GameData.h"
#include "data/Week.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

/* ------------------------------------------------------------------ */
/* Estado                                                              */
/* ------------------------------------------------------------------ */

// cena
static char songDir[256];
static char returnToEditor, returnToWeek, diffSuf[16], overrideP2[SONG_NAME_MAX];
static char restarted;
static char warnScreen;   // tela de avisos aberta (antes da contagem)
static float lastAppliedMusicVolume = -1.0f;

// musica (a batida e o tempo ficam em PlayStateBeat)
static Song song;

// cenario
static PlayStateStage stage;

// vida: taxas por evento, decididas uma vez por cena (ver ConfigureHealthRates)
static float oppDrain;   // perda por nota do oponente (0 = drain desligado)
static float botGain;    // ganho por nota do botplay (0 = desligado)

static float NoteVelocity(void) {
    return gameOptions.scroll == 1 ? 450.0f : -450.0f;
}

static PlayStateNoteView NoteView(void) {
    return (PlayStateNoteView) {
        .time = PlayStateBeat_Time(),
        .songSpeed = song.speed,
        .scrollVelocity = NoteVelocity(),
        .paused = PlayStatePause_IsPaused(),
        .dadStrums = PlayStateStrums_Dad(),
        .bfStrums = PlayStateStrums_Bf(),
    };
}

static const char* OpponentName(void) {
    return overrideP2[0] != 0 ? overrideP2 : song.player2;
}

/* ------------------------------------------------------------------ */
/* Eventos (chamados pelos modulos NoteSpawn e PlayerInput)            */
/* Os que so mexem em animacao ou popup vao direto para o modulo dono. */
/* ------------------------------------------------------------------ */

static void Event_OpponentHit(Note* note) {
    if (oppDrain > 0.0f)
        PlayStateHealth_Add(-oppDrain);
    PlayStateAnimations_OpponentHit(note->id);
    PlayStateStrums_ConfirmDad(note->id);
}

static void Event_BotHit(Note* note) {
    PlayStatePlayerInput_AddScore(350); // sick fixo no botplay
    if (botGain > 0.0f)
        PlayStateHealth_Add(botGain);
    PlayStateAnimations_BotHit(note->id);
    PlayStateStrums_ConfirmBf(note->id);
}

static void Event_PlayerHold(void) {
    PlayStateHealth_Add(0.04f * RayGame_DeltaTime());
    PlayStateAnimations_PlayerHold();
}

static void Event_PlayerHit(Note* note, PlayerRating rating) {
    PlayStateRatingPopups_Show(rating);
    PlayStateAnimations_PlayerHit(note->id);
}

static void SetupEvents(void) {
    static const PlayStateNoteEvents noteEvents = {
        .opponentHit = Event_OpponentHit,
        .botHit = Event_BotHit,
        .opponentHold = PlayStateAnimations_OpponentHold,
        .botHold = PlayStateAnimations_BotHold,
        .playerHold = Event_PlayerHold,
        .miss = PlayStatePlayerInput_MissNote,
    };
    static const PlayStatePlayerInputEvents inputEvents = {
        .hit = Event_PlayerHit,
        .miss = PlayStateAnimations_PlayerMiss,
    };

    PlayStateNoteSpawn_SetEvents(&noteEvents);
    PlayStatePlayerInput_SetEvents(&inputEvents);
}

/* ------------------------------------------------------------------ */
/* Volume                                                              */
/* ------------------------------------------------------------------ */

// Propaga o volume global (Game.c) para todas as faixas; so age quando o valor muda.
static void ApplyMusicVolume(void) {
    const float volume = RayGame_MusicVolumeLevel();

    if (volume == lastAppliedMusicVolume)
        return;

    lastAppliedMusicVolume = volume;

    PlayStateAudio_SetVolume(volume);
    PlayStateDeath_SetVolume(volume);
}

/* ------------------------------------------------------------------ */
/* Create                                                              */
/* ------------------------------------------------------------------ */

// Le as opcoes de drain/ganho uma vez por cena, em vez de a cada nota acertada.
static void ConfigureHealthRates(void) {
    const char drainOpp = gameOptions.botplay ? gameOptions.healthDrainBotplay : gameOptions.healthDrainPlay;
    oppDrain = drainOpp ? HEALTH_DRAIN_OPP : 0.0f;
    botGain = gameOptions.healthDrainBotplay ? HEALTH_GAIN_HIT : 0.0f;
}

static void ResetState(void) {
    ConfigureHealthRates();
    lastAppliedMusicVolume = -1.0f;

    PlayStatePlayerInput_Reset();
    PlayStateText_Reset();
    PlayStateAudio_Reset();
    PlayStateHealth_Reset();
    PlayStateCamera_Reset(PlayStateAnimations_Dad());
    PlayStateRatingPopups_Reset();
    PlayStatePause_Reset();
    PlayStateDeath_Reset();
}

// Posicao inicial da camera: o oponente (ja carregado).
static Vector2 DadPosition(void) {
    return PlayStateAnimations_Dad()->object.position;
}

// Restart: reaproveita tudo que ja esta carregado.
static void Restart(void) {
    PlayStateAnimations_Restart();
    PlayStateNoteSpawn_Reset();

    PlayStateAudio_Load(songDir);
    PlayStateAudio_StartPaused();

    PlayStateBeat_Start(&song);

    const Vector2 dad = DadPosition();
    PlayStateCamera_SetPosition(dad.x + SCREEN_W / 2, dad.y + SCREEN_H / 2);

    PlayStateIntro_Restart();
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

// Logo depois do chart (que diz quais personagens/stage): decodifica os PNG em threads de fundo
// (a parte lenta) enquanto sons etc. carregam. Os Load abaixo so enviam a imagem pronta para a GPU.
static void PrefetchAssets(void) {
    PlayStateAnimations_Prefetch(&song, songDir, OpponentName()); // personagens primeiro (PNGs maiores)
    PlayStateStage_Prefetch(song.stage, songDir);
}

// Sons comuns (acerto, erro, contagem, morte) ficam carregados entre as musicas.
static char commonSoundsLoaded;

static void LoadSounds(void) {
    if (commonSoundsLoaded) return;
    commonSoundsLoaded = 1;

    PlayStateAnimations_LoadSounds();
    PlayStateIntro_LoadSounds();
    PlayStateDeath_LoadSounds();
}

// Medicao do carregamento: cada etapa soma o tempo desde a anterior; o resumo sai no log.
static double loadLast;
static double loadTexStart;
static char loadReport[400];
static int loadLen;

static void LoadMark_Begin(void) {
    loadLast = GetTime();
    loadTexStart = Render_TextureTime();

    // Nome da musica = ultima pasta de songDir (para comparar os logs).
    const char* name = songDir;
    size_t n = strlen(songDir);
    while (n > 0 && (songDir[n - 1] == '/' || songDir[n - 1] == '\\')) n--;
    for (size_t i = 0; i < n; i++)
        if (songDir[i] == '/' || songDir[i] == '\\') name = songDir + i + 1;
    n -= (size_t)(name - songDir);
    loadLen = snprintf(loadReport, sizeof(loadReport), "LOAD [%.*s]", (int)n, name);
}

static void LoadMark(const char* step) {
    double now = GetTime();
    if (loadLen > 0 && loadLen < (int)sizeof(loadReport))
        loadLen += snprintf(loadReport + loadLen, sizeof(loadReport) - (size_t)loadLen, " %s: %.1f ms,", step, (now - loadLast) * 1000.0);
    loadLast = now;
}

static void LoadMark_End(double start) {
    char tail[96];
    snprintf(tail, sizeof(tail), " (textures on main thread: %.1f ms) total: %.1f ms",
             (Render_TextureTime() - loadTexStart) * 1000.0, (GetTime() - start) * 1000.0);
    if (loadLen > 0 && loadLen + (int)strlen(tail) < (int)sizeof(loadReport))
        memcpy(loadReport + loadLen, tail, strlen(tail) + 1);
    Log_Info(loadReport);
}

static void PlayState_Create([[maybe_unused]] RayScene* scene) {
    ResetState();

    warnScreen = 0;
    if (restarted) {
        Restart();
        return;
    }

    const double loadStart = GetTime();
    LoadMark_Begin();
    Log_WarnClear();
    // Botplay nunca morre (com ou sem drain): a vida so cai ate 0.
    PlayStateHealth_Init(gameOptions.scroll == 1, !gameOptions.botplay);
    PlayStateText_Init(songDir);

    PlayStateNoteSpawn_Init();
    SetupEvents();
    LoadMark("init");

    LoadSong();
    // Skin de notas desta musica (vazia = padrao). Sempre definida, para nao herdar a da anterior.
    // noteSkin (nosso) tem prioridade; senao o arrowSkin do Psych ("@" = imagem em images/).
    char skinName[SONG_NAME_MAX + 2] = "";
    if (song.noteSkin[0] != 0)
        snprintf(skinName, sizeof(skinName), "%s", song.noteSkin);
    else if (song.arrowSkin[0] != 0)
        snprintf(skinName, sizeof(skinName), "@%s", song.arrowSkin);
    Note_SetCurrentSkin(skinName[0] != 0 ? Cache_GetNoteSkinPtr(skinName, songDir) : NULL);
    PlayStateStrums_Load();   // depois da skin: os receptores usam as animacoes dela
    LoadMark("chart");
    PrefetchAssets();
    LoadSounds();               // roda enquanto o prefetch decodifica
    LoadMark("sounds");
    PlayStateAnimations_Load(&song, OpponentName());
    LoadMark("characters");

    PlayStateStage_Load(&stage, song.stage, songDir,
                        PlayStateAnimations_Gf(), PlayStateAnimations_Dad(), PlayStateAnimations_Bf());
    ImagePrefetch_Clear();      // descarta o que ninguem usou
    if (stage.hideGirlfriend)
        PlayStateAnimations_HideGirlfriend();
    LoadMark("stage");

    PlayStateAudio_Load(songDir);
    LoadMark("song audio");
    PlayStateNoteSpawn_Load(&song);
    PlayStateAudio_StartPaused();
    LoadMark("notes");

    PlayStateBeat_Start(&song);
    const Vector2 dad = DadPosition();
    PlayStateCamera_SetPosition(dad.x, dad.y);

    PlayStateIntro_LoadUi(PlayStateRatingPopups_LoadUi());
    LoadMark("ui");

    PlayStateCamera_SetGameZoom(stage.defaultCameraZoom);
    PlayStateCamera_SetSpeed(stage.cameraSpeed);

    // Aquece os streams de audio antes do primeiro frame.
    ClearBackground(BLACK);
    for (int i = 0; i < 10; i++)
        PlayStateAudio_UpdateStreams();
    LoadMark("warmup");
    LoadMark_End(loadStart);

    warnScreen = Log_WarnCount() > 0;
}

// Avisos de carregamento (stage/personagem/audio nao encontrado...). ENTER continua.
static void WarnScreen_Draw(void) {
    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color) {0, 0, 0, 215});

    char title[48];
    snprintf(title, sizeof(title), "AVISOS (%d)", Log_WarnCount());
    DrawTextEx(mainFont, title, (Vector2) {60, 50}, 44, 3, YELLOW);

    const int shown = Log_WarnCount() < LOG_WARN_MAX ? Log_WarnCount() : LOG_WARN_MAX;
    for (int i = 0; i < shown; i++)
        DrawTextEx(mainFont, Log_WarnAt(i), (Vector2) {60, 130.0f + (float)i * 30.0f}, 20, 1, WHITE);

    if (Log_WarnCount() > shown) {
        char more[64];
        snprintf(more, sizeof(more), "... e mais %d (veja assets/logs.txt)", Log_WarnCount() - shown);
        DrawTextEx(mainFont, more, (Vector2) {60, 130.0f + (float)shown * 30.0f}, 20, 1, GRAY);
    }

    DrawTextEx(mainFont, "ENTER para continuar", (Vector2) {60, SCREEN_H - 70}, 24, 2, GREEN);
}

/* ------------------------------------------------------------------ */
/* Saida de cena                                                       */
/* ------------------------------------------------------------------ */

// Volta ao Chart Editor (nao ao menu): o editor tem o proprio som, a musica do menu nao toca.
static char LeavesToEditor(char allowEditor) {
    return !returnToWeek && allowEditor && returnToEditor;
}

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

// Retorna 1 se a cena mudou (fim da musica).
static char UpdateSong(void) {
    PlayStateAudio_Update();

    PlayStateBeat_Update();

    if (!PlayStateAudio_IsPlaying()) {
        if (!LeavesToEditor(1)) {
            RayGame_ResetMusic();
            RayGame_ToggleMusic(1);
        }

        if (returnToWeek)
            FinishWeekSong();
        else
            LeaveGameplay(1);
        return 1;
    }

    PlayStatePlayerKeys keys;
    if (PlayStatePlayerInput_Update(PlayStateBeat_Time(), &keys))
        PlayStateStrums_ApplyInput(&keys);

    // Hits automaticos, hold e miss das notas (antes ficava no Draw).
    const PlayStateNoteView view = NoteView();
    PlayStateNoteSpawn_Update(&view);

    return 0;
}

static void PlayState_Update([[maybe_unused]] RayScene* scene) {
    const float dt = RayGame_DeltaTime();

    ApplyMusicVolume();

    if (warnScreen) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) warnScreen = 0;
        return;
    }

    if (!IsWindowFocused()) {
        PlayStateAudio_Pause();
        return;
    }

    if (PlayStatePause_IsPaused()) {
        const PlayStatePauseAction action = PlayStatePause_Update(dt);
        if (action == PLAYSTATE_PAUSE_RESTART) {
            restarted = 1;
            PlayState_SetScene();
        }
        else if (action == PLAYSTATE_PAUSE_EXIT) {
            if (!LeavesToEditor(1)) {
                RayGame_ResetMusic();
                RayGame_ToggleMusic(1);
            }
            LeaveGameplay(1);
        }
        return;
    }

    if (PlayStateHealth_IsDead()) {
        const PlayStateDeathAction action = PlayStateDeath_Update(dt, PlayStateAnimations_Bf());
        if (action == PLAYSTATE_DEATH_RESTART) {
            restarted = 1;
            PlayState_SetScene();
        }
        else if (action == PLAYSTATE_DEATH_LEAVE)
            LeaveGameplay(0);
    }
    else if (PlayStateBeat_InCountdown()) {
        PlayStateBeat_Countdown(dt);
    }
    else if (UpdateSong()) {
        return;
    }

    if (!PlayStateHealth_IsDead() && IsKeyPressed(KEY_ENTER))
        PlayStatePause_Open();

    PlayStateHealth_Update(dt);
    PlayStateCamera_Update(dt, stage.defaultCameraZoom);
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void DrawWorld(float dt) {
    PlayStateCamera_BeginGame();

    const char paused = PlayStatePause_IsPaused();
    PlayStateStage_DrawBack(&stage, paused);

    if (!paused) {
        PlayStateBeat_UpdateIdle(dt);
        PlayStateAnimations_Step();
    }

    PlayStateAnimations_Draw();

    PlayStateStage_DrawFront(&stage, paused);

    Render_StopCamera();
}

static void DrawHud(float dt) {
    PlayStateStrums_DrawBackdrop();
    PlayStateStrums_Draw();

    const PlayStateNoteView view = NoteView();
    PlayStateNoteSpawn_Draw(&view);

    PlayStateHealth_Draw(PlayStateAnimations_Dad()->icon, PlayStateAnimations_Bf()->icon);
    PlayStateText_Draw(PlayStatePlayerInput_Score(), PlayStatePlayerInput_Misses(), (float)PlayStateBeat_Time(),
                       gameOptions.showScore, gameOptions.scorePosition);

    if (PlayStateBeat_InCountdown())
        PlayStateIntro_Draw();

    PlayStateRatingPopups_Draw(dt);

    // Marca d'agua fixa: sem movimento, escala pulsante ou fade.
    if (gameOptions.botplay)
        PlayStateText_DrawBotplayMark();
}

static void PlayState_Draw([[maybe_unused]] RayScene* scene) {
    if (PlayStateHealth_IsDead()) {
        Character* bf = PlayStateAnimations_Bf();
        ClearBackground(BLACK);

        PlayStateCamera_BeginGame();
        AnimatedObject_UpdateFrame(&bf->object);
        Render_DrawAnimatedObject(&bf->object);
        Render_StopCamera();
        return;
    }

    const float dt = RayGame_DeltaTime();

    ClearBackground(BLACK); // Psych: cor de fundo da camera e preta

    DrawWorld(dt);

    PlayStateCamera_BeginHUD();
    DrawHud(dt);

    if (PlayStatePause_IsPaused())
        PlayStatePause_Draw(LeavesToEditor(1));
    if (warnScreen)
        WarnScreen_Draw();
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

/* ------------------------------------------------------------------ */
/* Destroy e registro da cena                                          */
/* ------------------------------------------------------------------ */

static void PlayState_Destroy([[maybe_unused]] RayScene* scene) {
    PlayStateAudio_Unload();
    if (restarted) return; // restart reaproveita tudo

    // Sons comuns e ui continuam carregados (reuso na proxima musica); so garante que nada toca.
    PlayStateDeath_Stop();

    PlayStateNoteSpawn_Unload();
    Song_Free(&song);

    PlayStateAnimations_Unload();

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
