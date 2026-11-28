/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl outd day30 of10 2026 */
/*
 * PlayState.c - coordenador da gameplay.
 * Cada parte pesada vive em um modulo gameplay/PlayState*.c. Aqui ficam o estado da cena,
 * a ordem de carregamento, o fluxo (saida, fim da musica) e a ligacao entre os modulos
 * por callbacks. Batida, popup de rating e personagens tem modulo proprio.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/api/NoteSkinCache.h"
#include "scenes/gameplay/PlayState.h"
#include "scenes/modding/charteditor/ChartEditor.h"
#include "scenes/menu/freeplay/Freeplay.h"
#include "scenes/menu/storymode/StoryMode.h"
#include "logic/notes/Note.h"
#include "data/song/Song.h"
#include "scenes/gameplay/stage/PlayStateStage.h"
#include "scenes/gameplay/HUD/PlayStateHealth.h"
#include "scenes/gameplay/audio/PlayStateAudio.h"
#include "scenes/gameplay/HUD/PlayStateText.h"
#include "scenes/gameplay/notes/PlayStateNoteSpawn.h"
#include "scenes/gameplay/notes/PlayStatePlayerInput.h"
#include "scenes/gameplay/visual/PlayStateCamera.h"
#include "scenes/gameplay/notes/PlayStateStrums.h"
#include "scenes/gameplay/flow/PlayStatePause.h"
#include "scenes/gameplay/flow/PlayStateChangers.h"
#include "scenes/gameplay/flow/PlayStateDeath.h"
#include "scenes/gameplay/flow/PlayStateIntro.h"
#include "scenes/gameplay/characters/PlayStateBeat.h"
#include "scenes/gameplay/characters/PlayStateAnimations.h"
#include "scenes/gameplay/HUD/PlayStateRatingPopups.h"
#include "scenes/gameplay/visual/PlayStateNoteSplash.h"
#include "logic/notes/NoteTypes.h"
#include "scenes/gameplay/events/PlayStateEventsInGameplay.h"
#include "core/Log.h"
#include "core/ModPath.h"
#include "data/dialogue/Dialogue.h"
#include "logic/dialogue/DialogueBox.h"
#include "scenes/Transition.h"
#include "rendering/cpu/ImagePrefetch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Game.h"
#include "GameData.h"
#include "data/week/Week.h"
#include "rlgl.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f
#define PAUSE_SEEK_SECS 5.0   /* passo das setas na opcao Skip time da pausa */

/* ------------------------------------------------------------------ */
/* Estado                                                             */
/* ------------------------------------------------------------------ */

// cena
static char songDir[256];
static char returnToEditor, returnToWeek, diffSuf[16], overrideP2[SONG_NAME_MAX];
static char restarted;
static float lastAppliedMusicVolume = -1.0f;
static PlayStateTestMode testMode = PLAYSTATE_TEST_FULL;  // so o test play do chart editor muda isto

// musica (a batida e o tempo ficam em PlayStateBeat)
static Song song;

// dialogo da Psych antes da contagem (dialogue.json da musica)
static char dialogueOn;
static DialogueFile dialogueFile;

// cenario
static PlayStateStage stage;

// vida: taxas por evento, decididas uma vez por cena (ver ConfigureHealthRates)
static float oppDrain;   // perda por nota do oponente (0 = drain desligado)
static float botGain;    // ganho por nota do botplay (0 = desligado)

// Foto da cena para a pausa: mundo + HUD desenhados uma vez (ao abrir e depois de seek).
// Pausado, o Draw so cola a foto e desenha o menu; o loop pula o draw quando nada muda.
static RenderTexture2D pauseShot;
static char pauseShotOk;

static float NoteVelocity(void) {
    return SCROLL_IS_DOWN(gameOptions.scroll) ? 450.0f : -450.0f;
}

static PlayStateNoteView NoteView(void) {
    return (PlayStateNoteView) {
        .time = PlayStateBeat_Time(),
        .songSpeed = PlayStateChangers_SongSpeed(song.speed, PlayStateEvents_ScrollMult()),   // Scroll Type/Speed + Change Scroll Speed
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
        PlayStateHealth_Drain(oppDrain);   // drain nunca mata: para em 0
    PlayStateAnimations_OpponentHit(note);
    PlayStateStrums_ConfirmDad(note->id);
}

/* Acerto automatico do lado do BF (botplay, ou a IA no Play as oponente). */
static void Event_BotHit(Note* note) {
    if (gameOptions.botplay) {
        PlayStatePlayerInput_AddScore(350); // sick fixo no botplay
        PlayStatePlayerInput_CountHit(PLAYER_RATING_SICK);
        if (botGain > 0.0f)
            PlayStateHealth_Add(botGain);
    }
    else if (oppDrain > 0.0f)
        PlayStateHealth_Drain(oppDrain);    // a IA do BF acertando desconta a vida, como o oponente
    PlayStateAnimations_BotHit(note);
    PlayStateStrums_ConfirmBf(note->id);
    if (!NoteTypes_Get(note->type)->noSplash)
        PlayStateNoteSplash_Spawn(note->id, 1);
}

static void Event_PlayerHold(const Note* note) {
    PlayStateHealth_AddForSide(0.04f * RayGame_DeltaTime(), (char)note->mustHit);
    if (note->mustHit) PlayStateAnimations_PlayerHold(note);
    else PlayStateAnimations_OpponentHold(note);   // jogador no lado do oponente (Co-Op ou Play as)
}

static void Event_PlayerHit(Note* note, PlayerRating rating) {
    PlayStateRatingPopups_Show(rating);
    PlayStateText_Bump();
    if (note->mustHit) PlayStateAnimations_PlayerHit(note);
    else PlayStateAnimations_OpponentHit(note);
    if (!NoteTypes_Get(note->type)->noSplash)
        PlayStateNoteSplash_Spawn(note->id, note->mustHit);   // splash no receptor do lado que acertou
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
    PlayStateAnimations_SetHitVolume(volume);
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
    PlayStateNoteSplash_Reset();
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
    PlayStateEvents_Reset();

    const Vector2 dad = DadPosition();
    PlayStateCamera_SetPosition(dad.x + SCREEN_W / 2, dad.y + SCREEN_H / 2);

    PlayStateIntro_Restart();
    restarted = 0;
}

// Janela de acerto por dificuldade, em tempo real: vezes a Song speed (em segundos de musica).
static void ApplyNoteWindow(void) {
    float window = 0.200f;
    if (strcmp(diffSuf, "easy") == 0) window = 0.240f;
    else if (strcmp(diffSuf, "hard") == 0) window = 0.180f;
    Note_SetWindow(window * PlayStateAudio_Speed());
}

static void LoadSong(void) {
    if (diffSuf[0] != 0)
        Song_LoadSongFromDirDiff(&song, songDir, diffSuf, gameOptions.streamingJsonParse);
    else
        Song_LoadSongFromDir(&song, songDir, gameOptions.streamingJsonParse);

    ApplyNoteWindow();
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

// Psych (script padrao): dialogue.json so no Story Mode e so na primeira vez (o restart pula).
static void StartDialogue(void) {
    dialogueOn = 0;
    if (!returnToWeek || testMode != PLAYSTATE_TEST_FULL) return;

    char path[512];
    if (!Dialogue_FindForSong(songDir, path, sizeof(path)) || !Dialogue_Load(path, &dialogueFile) ||
        dialogueFile.count == 0)
        return;

    char root[512];
    DialogueBox_Begin(ModPath_Root(songDir, root, sizeof(root)) ? root : NULL);
    DialogueBox_Start(&dialogueFile);
    dialogueOn = 1;
}

static void PlayState_Create([[maybe_unused]] RayScene* scene) {
    ResetState();

    if (restarted) {
        Restart();
        return;
    }

    const double loadStart = GetTime();
    PlayStateAudio_SetSpeed(1.0f); // musica nova comeca em 1x (o restart mantem a Song speed)
    PlayStateHealth_SetMult(HEALTH_MULT_GAIN, 1.0f);   // idem para os multiplicadores de vida
    PlayStateHealth_SetMult(HEALTH_MULT_LOSS, 1.0f);
    PlayStateChangers_Reset();                         // e Scroll Type/Speed, Instakill
    LoadMark_Begin();
    Log_WarnClear();
    // Botplay nunca morre (com ou sem drain): a vida so cai ate 0.
    PlayStateHealth_Init(SCROLL_IS_DOWN(gameOptions.scroll), !gameOptions.botplay && !gameOptions.coopOn); /* Co-Op: imortal */
    PlayStateText_Init(songDir);

    PlayStateNoteSpawn_Init();
    SetupEvents();
    LoadMark("init");

    LoadSong();
    PlayStatePause_SetInfo(songDir, diffSuf);
    PlayStateEvents_Load(songDir, diffSuf);
    // Skin de notas desta musica (vazia = padrao). Sempre definida, para nao herdar a da anterior.
    // noteSkin (nosso) tem prioridade; senao o arrowSkin do Psych ("@" = imagem em images/).
    char skinName[SONG_NAME_MAX + 2] = "";
    if (song.noteSkin[0] != 0)
        snprintf(skinName, sizeof(skinName), "%s", song.noteSkin);
    else if (song.arrowSkin[0] != 0)
        snprintf(skinName, sizeof(skinName), "@%s", song.arrowSkin);
    Note_SetCurrentSkin(skinName[0] != 0 ? Cache_GetNoteSkinPtr(skinName, songDir) : NULL);
    PlayStateNoteSpawn_LoadTypes(songDir);   // custom_notetypes + textura de cada tipo (Hurt Note...)
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
    Song_ReleaseNotes(&song);   /* as notas agora vivem no array da gameplay */
    PlayStateAudio_StartPaused();
    LoadMark("notes");

    PlayStateNoteSplash_Load();
    PlayStateBeat_Start(&song);
    const Vector2 dad = DadPosition();
    PlayStateCamera_SetPosition(dad.x, dad.y);

    PlayStateIntro_LoadUi(PlayStateRatingPopups_LoadUi());
    StartDialogue();
    LoadMark("ui");

    PlayStateCamera_SetGameZoom(stage.defaultCameraZoom);
    PlayStateCamera_SetSpeed(stage.cameraSpeed);

    // Aquece os streams de audio antes do primeiro frame.
    ClearBackground(BLACK);
    for (int i = 0; i < 10; i++)
        PlayStateAudio_UpdateStreams();
    LoadMark("warmup");
    LoadMark_End(loadStart);
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

// Seek pedido pela pausa (Skip time). Nao age na contagem nem sem musica; limita a [0, duracao).
static void SeekBy(double deltaSecs) {
    const float length = PlayStateAudio_Length();
    if (PlayStateBeat_InCountdown() || length <= 0.0f) return;

    double target = PlayStateBeat_Time() + deltaSecs;
    if (target < 0.0) target = 0.0;
    if (target > (double)length - 0.05) target = (double)length - 0.05;

    PlayStateAudio_SeekTo(target);            // inst e vozes; mantem a pausa
    PlayStateBeat_SeekTo(target);             // passo, secao, bpm e camera
    PlayStateEvents_SeekTo(target);           // eventos (fades) do events.json
    PlayStateNoteSpawn_SeekTo((float)target); // notas: anteriores resolvidas sem score
}

// Retorna 1 se a cena mudou (fim da musica).
static char UpdateSong(void) {
    PlayStateAudio_Update();

    PlayStateBeat_Update();
    PlayStateEvents_Update(PlayStateBeat_Time());

    if (!PlayStateAudio_IsPlaying()) {
        // Na semana a musica do menu continua pausada entre as musicas (e na tela da semana).
        if (!LeavesToEditor(1) && !returnToWeek) {
            RayGame_ResetMusic();
            RayGame_ToggleMusic(1);
        }

        if (returnToWeek)
            FinishWeekSong();
        else
            LeaveGameplay(1);
        return 1;
    }

    PlayStatePlayerKeys keys[2] = {{{0}}};   // botplay nao preenche: fica zerado e o monitor apaga
    if (PlayStatePlayerInput_Update(PlayStateBeat_Time(), keys)) {
        for (int slot = 0; slot < PlayStatePlayerInput_Slots(); slot++)
            PlayStateStrums_ApplyInput(&keys[slot], PlayStatePlayerInput_SlotSideBf(slot));
    }
    PlayStateStrums_UpdateGlow(keys);

    // Hits automaticos, hold e miss das notas (antes ficava no Draw).
    const PlayStateNoteView view = NoteView();
    PlayStateNoteSpawn_Update(&view);

    return 0;
}

static void DrawScene(float dt);

// Desenha a cena (sem a pausa) na foto. Roda no Update: aqui ainda nao ha render target ativo.
static void CapturePauseShot(void) {
    if (pauseShot.id == 0) pauseShot = LoadRenderTexture((int)SCREEN_W, (int)SCREEN_H);
    pauseShotOk = IsRenderTextureValid(pauseShot);
    if (!pauseShotOk) return; // sem RT: o Draw desenha a cena inteira (caminho antigo)

    BeginTextureMode(pauseShot);
    DrawScene(0.0f); // dt 0: splash e popups nao andam
    EndTextureMode();
}

static void OpenPause(void) {
    PlayStatePause_Open();
    CapturePauseShot();
}

// Input da pausa. Retorna 1 se a cena mudou.
static char UpdatePause(float dt) {
    const PlayStatePauseAction action = PlayStatePause_Update(dt);
    switch (action) {
        case PLAYSTATE_PAUSE_RESTART:
            restarted = 1;
            PlayState_SetScene();
            return 1;
        case PLAYSTATE_PAUSE_EXIT:
            if (!LeavesToEditor(1) && !returnToWeek) {
                RayGame_ResetMusic();
                RayGame_ToggleMusic(1);
            }
            LeaveGameplay(1);
            return 1;
        case PLAYSTATE_PAUSE_SEEK_BACK:
        case PLAYSTATE_PAUSE_SEEK_FORWARD:
            SeekBy(action == PLAYSTATE_PAUSE_SEEK_BACK ? -PAUSE_SEEK_SECS : PAUSE_SEEK_SECS);
            CapturePauseShot(); // notas e camera mudaram
            return 0;
        case PLAYSTATE_PAUSE_SPEED:
            ApplyNoteWindow(); // janela de acerto segue a velocidade
            return 0;
        case PLAYSTATE_PAUSE_SCROLL:
            CapturePauseShot(); // notas mudaram de lugar
            return 0;
        default:
            return 0;
    }
}

static void PlayState_Update(RayScene* scene) {
    const float dt = RayGame_DeltaTime();

    ApplyMusicVolume();

    // Textos do HUD rasterizam aqui (so os que mudaram). Nao pode ser no Draw: la a cena
    // ja esta dentro de render target (tela cheia) e o raster aninhado quebra o framebuffer.
    PlayStateText_Refresh(PlayStatePlayerInput_Score(), PlayStatePlayerInput_Misses(), PlayStatePlayerInput_Accuracy(),
                          (float)PlayStateBeat_Time(),
                          gameOptions.showScore, gameOptions.scorePosition);

    if (!IsWindowFocused()) {
        PlayStateAudio_Pause();
        return;
    }

    if (PlayStatePause_IsPaused()) {
        if (UpdatePause(dt)) return; // cena trocada: `scene` ja nao e desta tela
        if (PlayStatePause_TakeRedraw()) RayScene_RequestRedraw(scene);
        scene->staticWhenIdle = PlayStatePause_IsPaused(); // parado: so desenha quando o menu muda
        if (!scene->staticWhenIdle) pauseShotOk = 0;        // Resume
        return;
    }

    const char inDialogue = dialogueOn; // o Enter que fecha o dialogo nao abre a pausa
    if (PlayStateHealth_IsDead()) {
        const PlayStateDeathAction action = PlayStateDeath_Update(dt, PlayStateAnimations_Bf());
        if (action == PLAYSTATE_DEATH_RESTART) {
            restarted = 1;
            PlayState_SetScene();
        }
        else if (action == PLAYSTATE_DEATH_LEAVE)
            LeaveGameplay(0);
    }
    else if (dialogueOn) {
        const bool accept = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
        const bool back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
        if (DialogueBox_Update(dt, accept, back)) {
            DialogueBox_End();
            dialogueOn = 0;
        }
    }
    else if (PlayStateBeat_InCountdown()) {
        PlayStateBeat_Countdown(dt * PlayStateAudio_Speed()); // contagem no ritmo da musica
    }
    else if (UpdateSong()) {
        return;
    }

    if (!inDialogue && !PlayStateHealth_IsDead() && IsKeyPressed(KEY_ENTER)) {
        OpenPause();
        scene->staticWhenIdle = true;
    }

    PlayStateHealth_Update(dt);
    PlayStateCamera_Update(dt, stage.defaultCameraZoom);
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */

static void DrawWorld(float dt) {
    const char paused = PlayStatePause_IsPaused();
    if (!PlayStateEvents_WorldVisible()) {   // Set Property camGame.visible = false: so anima
        if (!paused) {
            PlayStateBeat_UpdateIdle(dt);
            PlayStateAnimations_Step();
        }
        return;
    }

    PlayStateCamera_BeginGame();

    PlayStateStage_DrawBack(&stage, paused);

    if (!paused) {
        PlayStateBeat_UpdateIdle(dt);
        PlayStateAnimations_Step();
    }

    PlayStateAnimations_Draw();

    PlayStateStage_DrawFront(&stage, paused);

    Render_StopCamera();
}

/* Nome do jogador no Co-Op (vazio = "Player N"). */
static void CoopName(int slot, char* out, size_t size) {
    if (gameOptions.coopName[slot][0] != 0) (void)snprintf(out, size, "%s", gameOptions.coopName[slot]);
    else (void)snprintf(out, size, "Player %d", slot + 1);
}

static void DrawHud(float dt) {
    if (dialogueOn) { // Psych: receptores so aparecem na contagem
        PlayStateHealth_Draw(PlayStateAnimations_Dad()->icon, PlayStateAnimations_Bf()->icon);
        PlayStateText_Draw(gameOptions.showScore, gameOptions.scorePosition);
        return;
    }
    PlayStateStrums_DrawBackdrop();
    PlayStateStrums_Draw();
    if (gameOptions.coopOn) {
        char name[2][COOP_NAME_MAX];
        CoopName(0, name[0], sizeof(name[0]));
        CoopName(1, name[1], sizeof(name[1]));
        const int bfSlot = PlayStatePlayerInput_SlotSideBf(0) ? 0 : 1;   // jogador 1 ou 2 no lado do BF
        PlayStateStrums_DrawSideNames(name[bfSlot], name[1 - bfSlot]);
    }
    PlayStateNoteSplash_Draw(dt);

    const PlayStateNoteView view = NoteView();
    PlayStateNoteSpawn_Draw(&view);
    PlayStateStrums_DrawKeyMonitor();

    PlayStateHealth_Draw(PlayStateAnimations_Dad()->icon, PlayStateAnimations_Bf()->icon);
    PlayStateText_Draw(gameOptions.showScore, gameOptions.scorePosition);

    if (PlayStateBeat_InCountdown())
        PlayStateIntro_Draw();

    PlayStateRatingPopups_Draw(dt);

    // Marca d'agua fixa: sem movimento, escala pulsante ou fade.
    if (gameOptions.botplay)
        PlayStateText_DrawBotplayMark();
}

// Menu de pausa com o tempo atual e total (Skip time). Por cima da foto, sem camera.
static void DrawPause(void) {
    if (pauseShotOk) {
        // Copia sem blend: a foto ja tem o alfa final da cena (com blend o fundo escureceria duas vezes).
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTextureRec(pauseShot.texture, (Rectangle) {0.0f, 0.0f, SCREEN_W, -SCREEN_H}, (Vector2) {0.0f, 0.0f}, WHITE);
        EndBlendMode();
    }
    const double now = PlayStateBeat_Time();
    PlayStatePause_Draw(LeavesToEditor(1), now > 0.0 ? (float)now : 0.0f, PlayStateAudio_Length());
}

// Modo "so receptores" (F6 no chart editor): fundo preto, so receptores e notas. Sem HUD/personagens.
static void DrawReceptorsOnly(void) {
    ClearBackground(BLACK);
    PlayStateCamera_BeginHUD();
    PlayStateStrums_Draw();
    const PlayStateNoteView view = NoteView();
    PlayStateNoteSpawn_Draw(&view);
    Render_StopCamera();
}

// Cena sem o menu de pausa (tambem usada para a foto da pausa).
static void DrawScene(float dt) {
    if (testMode == PLAYSTATE_TEST_RECEPTORS) {
        DrawReceptorsOnly();
        return;
    }

    ClearBackground(BLACK); // Psych: cor de fundo da camera e preta

    DrawWorld(dt);

    PlayStateCamera_BeginHUD();
    PlayStateEvents_DrawLetterbox(); // barras antes do HUD: notas ficam visiveis
    if (PlayStateEvents_HudVisible()) DrawHud(dt);   // Set Property camHUD.visible
    PlayStateEvents_Draw(); // fades do events.json cobrem a tela (HUD incluso)
    if (dialogueOn) DialogueBox_Draw();
    Render_StopCamera();
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

    if (PlayStatePause_IsPaused()) {
        if (!pauseShotOk) DrawScene(0.0f); // sem foto: cena inteira parada
        DrawPause();
    }
    else
        DrawScene(RayGame_DeltaTime());

    if (testMode != PLAYSTATE_TEST_RECEPTORS)
        Transition_Draw(); // veu de entrada vindo da loading (no-op depois)
}

/* ------------------------------------------------------------------ */
/* API publica (declarada no header de cada cena)                              */
/* ------------------------------------------------------------------ */

void PlayState_SetSongDir(const char* dir) {
    PlayState_SetSongDirKeep(dir);
    returnToEditor = 0;
    returnToWeek = 0;
    diffSuf[0] = 0;
    overrideP2[0] = 0;
    PlayState_SetTestMode(PLAYSTATE_TEST_FULL);
}

void PlayState_SetTestMode(PlayStateTestMode mode) {
    testMode = mode;
    switch (mode) {
        case PLAYSTATE_TEST_INST:       PlayStateAudio_SetMix(PSA_MIX_INST); break;
        case PLAYSTATE_TEST_VOCALS:     PlayStateAudio_SetMix(PSA_MIX_VOCALS); break;
        case PLAYSTATE_TEST_BF_VOCALS:  PlayStateAudio_SetMix(PSA_MIX_BF); break;
        case PLAYSTATE_TEST_OPP_VOCALS: PlayStateAudio_SetMix(PSA_MIX_OPP); break;
        case PLAYSTATE_TEST_FULL:
        case PLAYSTATE_TEST_RECEPTORS:
        default:                        PlayStateAudio_SetMix(PSA_MIX_FULL); break;
    }
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
    pauseShotOk = 0;
    if (dialogueOn) {
        DialogueBox_End();
        dialogueOn = 0;
    }
    if (restarted) return; // restart reaproveita tudo

    if (pauseShot.id != 0) UnloadRenderTexture(pauseShot);
    pauseShot = (RenderTexture2D) {0};

    // Sons comuns e ui continuam carregados (reuso na proxima musica); so garante que nada toca.
    PlayStateDeath_Stop();

    PlayStateNoteSpawn_Unload();
    PlayStateEvents_Unload();
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
