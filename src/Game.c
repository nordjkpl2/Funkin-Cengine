/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * RayGame.c - Estado global, áudio, janela e loop principal.
 */

#include "Game.h"
#include "GameData.h"
#include "core/Log.h"
#include "threads/cpu/Jobs.h"
#include "data/modimport/ModImport.h"
#include "threads/Thread.h"
#include "rendering/Display.h"
#include "rendering/shaders/ShaderFx.h"
#include "rendering/cpu/ImagePrefetch.h"
#if defined(FNF_DEBUG_STATS)
#include "rlgl.h"
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define RAYGAME_CANVAS_WIDTH  1280
#define RAYGAME_CANVAS_HEIGHT 720
#define RAYGAME_DEFAULT_FPS   60

/* ================================================================
   1. ESTADO GLOBAL
   ================================================================ */

RayGame game;

static float dt = 0.0f, zoom = 1.0f, musicVolume = 1.0f, musicBaseVolume = 1.0f, volOsdTimer = 0.0f;
static int width = RAYGAME_CANVAS_WIDTH, height = RAYGAME_CANVAS_HEIGHT, targetFps = RAYGAME_DEFAULT_FPS;
static int cachedVolumePercent = 100;

static bool vsyncEnabled = false, viewportInitialized = false, lastFillScreen = false;
static bool audioReady = false, gameDataReady = false, musicLoaded = false, playingMusic = false, stretchReady = false;

Music gameMusic = {0};
static RenderTexture2D stretchTarget = {0};
static char volumeText[24] = "VOLUME 100%";

/* ================================================================
   2. UTILITÁRIOS INTERNOS
   ================================================================ */

static inline float RayGame_Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static inline void RayGame_UpdateVolumeText(void)
{
    const int percent = (int)(musicVolume * 100.0f + 0.5f);

    if (percent == cachedVolumePercent)
        return;

    cachedVolumePercent = percent;
    (void)snprintf(volumeText, sizeof(volumeText), "VOLUME %d%%", percent);
}

static void RayGame_UpdateViewport(bool force)
{
    const bool fillScreen = (gameOptions.fillScreen != 0);
    const bool resized = IsWindowResized();

    if (!force && viewportInitialized && !resized && fillScreen == lastFillScreen)
        return;

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    if (screenWidth > 0) width = screenWidth;
    if (screenHeight > 0) height = screenHeight;

    if (fillScreen)
    {
        zoom = 1.0f;
    }
    else
    {
        const float scaleX = (float)width / (float)RAYGAME_CANVAS_WIDTH;
        const float scaleY = (float)height / (float)RAYGAME_CANVAS_HEIGHT;
        zoom = (scaleX < scaleY) ? scaleX : scaleY;
    }

    lastFillScreen = fillScreen;
    viewportInitialized = true;
}

#if defined(FNF_DEBUG_STATS)
/* Overlay de debug (F3): so existe se compilar com -DFNF_DEBUG_STATS (cmake -DFNF_DEBUG_STATS=ON). */
static struct { char on; double upd, draw, end; unsigned int calls, flushes; } dbg = { .on = 0 };

static inline void RayGame_EndFrame(double tDraw0)
{
    if (!dbg.on) { EndDrawing(); return; }
    {
        const double t1 = GetTime();
        EndDrawing();
        dbg.draw = (t1 - tDraw0) * 1000.0;
        dbg.end = (GetTime() - t1) * 1000.0;
        rlTakeDrawStats(&dbg.calls, &dbg.flushes);
    }
}
#else
#define RayGame_EndFrame(tDraw0) ((void)(tDraw0), EndDrawing())
#endif

static inline void RayGame_UpdateOverlay(float frameDelta)
{
    if (volOsdTimer > 0.0f)
    {
        volOsdTimer -= frameDelta;
        if (volOsdTimer < 0.0f)
            volOsdTimer = 0.0f;
    }
}

/* Ociosidade: cena marcada como staticWhenIdle pode pular o draw. Input sempre pede redesenho. */
#define RAYGAME_IDLE_SLEEP_S   0.008f  /* dorme entre frames ociosos (sem V-SYNC, o loop nao bloqueia) */
#define RAYGAME_IDLE_REDRAW_S  0.25f   /* redesenho forcado mesmo parado (janela exposta, texturas async) */

static inline bool RayGame_HadInput(void)
{
    const Vector2 md = GetMouseDelta();
    if (md.x != 0.0f || md.y != 0.0f || GetMouseWheelMove() != 0.0f) return true;
    for (int k = 0; k <= KEY_MENU; k++)
        if (IsKeyPressed(k) || IsKeyReleased(k)) return true;
    for (int b = 0; b < 8; b++)   /* raylib: MAX_MOUSE_BUTTONS = 8 (interno, nao exposto) */
        if (IsMouseButtonPressed(b) || IsMouseButtonReleased(b)) return true;
    return false;
}

/* Textura do rotulo de FPS (ver RayGame_DrawOverlay). */
#define FPS_LABEL_W 96
#define FPS_LABEL_H 36
static RenderTexture2D fpsLabel = {0};
static bool fpsLabelReady = false;

static inline void RayGame_DrawOverlay(void)
{
    if (gameOptions.showFps)
    {
        /* O rotulo (texto + sombra) so e desenhado de novo na textura quando o valor de FPS muda.
           Nos outros quadros so se desenha essa textura: nenhum DrawText. */
        static int fpsShown = -1;

        if (!fpsLabelReady)
        {
            fpsLabel = LoadRenderTexture(FPS_LABEL_W, FPS_LABEL_H);
            fpsLabelReady = (fpsLabel.id != 0);
        }

        const int fps = GetFPS();
        if (fpsLabelReady && fps != fpsShown)
        {
            fpsShown = fps;
            Color fpsColor = SKYBLUE;   /* mesma logica de cor do DrawFPS do raylib */
            if (fps < 30 && fps >= 15) fpsColor = ORANGE;
            else if (fps < 15) fpsColor = RED;

            char fpsText[16];
            (void)snprintf(fpsText, sizeof(fpsText), "%2i FPS", fps);

            BeginTextureMode(fpsLabel);
            ClearBackground((Color){0, 0, 0, 0});
            DrawText(fpsText, 2, 2, 20, (Color){0, 0, 0, 160});
            DrawText(fpsText, 0, 0, 20, fpsColor);
            EndTextureMode();
        }

        if (fpsLabelReady)
            DrawTextureRec(fpsLabel.texture, (Rectangle){0.0f, 0.0f, (float)FPS_LABEL_W, -(float)FPS_LABEL_H},
                           (Vector2){10.0f, 3.0f}, WHITE);
    }

#if defined(FNF_DEBUG_STATS)
    if (dbg.on)
    {
        const Color dbgShadow = {0, 0, 0, 160};
        const char *dbgLine1 = TextFormat("upd %.2f  draw %.2f  end %.2f ms", dbg.upd, dbg.draw, dbg.end);
        const char *dbgLine2 = TextFormat("draw calls %u  flushes %u", dbg.calls, dbg.flushes);
        DrawText(dbgLine1, 12, 72, 18, dbgShadow);
        DrawText(dbgLine1, 10, 70, 18, YELLOW);
        DrawText(dbgLine2, 12, 94, 18, dbgShadow);
        DrawText(dbgLine2, 10, 92, 18, YELLOW);

        /* Falhas do prefetch de imagens (so as que deram errado; o motivo vai para o log). */
        const ImagePrefetchStats *pf = ImagePrefetch_Stats();
        const unsigned pfFails = pf->pathInvalid + pf->notPng + pf->tooBig + pf->overBudget
                               + pf->noSlot + pf->submitFailed + pf->decodeFailed;
        const char *dbgLine3 = TextFormat("prefetch falhas %u (orc %u, slot %u, decod %u, nPNG %u)",
                                          pfFails, pf->overBudget, pf->noSlot, pf->decodeFailed, pf->notPng);
        DrawText(dbgLine3, 12, 116, 18, dbgShadow);
        DrawText(dbgLine3, 10, 114, 18, pfFails > 0 ? ORANGE : YELLOW);
    }
#endif

    if (volOsdTimer > 0.0f)
    {
        DrawText(volumeText, 10, 26, 20, WHITE);

        /* Barra de volume em rampa: so as barras do volume atual (4 a 18 px, crescendo para a direita),
           coladas embaixo do texto, uma por 10%. Sem fundo nem barras vazias. */
        const int filled = (int)(musicVolume * 10.0f + 0.5f);
        for (int i = 0; i < filled; i++)
        {
            const int h = 4 + (i * 14) / 9;
            DrawRectangle(10 + i * 12, 66 - h, 8, h, WHITE);
        }
    }
}

/* ================================================================
   3. ÁUDIO
   ================================================================ */

void RayGame_SetMusic(const char *path, float volume, char looping)
{
    if (musicLoaded)
    {
        StopMusicStream(gameMusic);
        UnloadMusicStream(gameMusic);
        musicLoaded = false;
        playingMusic = false;
        gameMusic = (Music){0};
    }

    if (!audioReady || path == NULL || path[0] == '\0')
        return;

    gameMusic = LoadMusicStream(path);

    if (gameMusic.stream.buffer == NULL)
    {
        gameMusic = (Music){0};
        return;
    }

    musicLoaded = true;
    musicBaseVolume = RayGame_Clamp01(volume);
    gameMusic.looping = (looping != 0);

    SetMusicVolume(gameMusic, musicBaseVolume * musicVolume);
    PlayMusicStream(gameMusic);
    playingMusic = true;
}

void RayGame_ToggleMusic(char on)
{
    const bool enable = (on != 0);

    if (!musicLoaded || !audioReady || enable == playingMusic)
        return;

    if (enable) ResumeMusicStream(gameMusic);
    else PauseMusicStream(gameMusic);

    playingMusic = enable;
}

/* raylib para o stream sozinho quando um stream sem loop chega ao fim. */
char RayGame_MusicFinished(void)
{
    return musicLoaded && playingMusic && !IsMusicStreamPlaying(gameMusic);
}

void RayGame_ResetMusic(void)
{
    if (!musicLoaded || !audioReady)
        return;

    StopMusicStream(gameMusic);
    SeekMusicStream(gameMusic, 0.0f);
    PlayMusicStream(gameMusic);
    playingMusic = true;
}

static void RayGame_BumpMusicVolume(float delta)
{
    musicVolume = RayGame_Clamp01(musicVolume + delta);

    if (musicLoaded)
        SetMusicVolume(gameMusic, musicBaseVolume * musicVolume);

    RayGame_UpdateVolumeText();
    volOsdTimer = 2.0f;
}

float RayGame_MusicVolumeLevel(void)
{
    return musicVolume;
}

/* ================================================================
   4. TELA / FPS
   ================================================================ */

float RayGame_ZoomFactor(void)
{
    return zoom;
}

int RayGame_CanvasWidth(void)
{
    return RAYGAME_CANVAS_WIDTH;
}

int RayGame_CanvasHeight(void)
{
    return RAYGAME_CANVAS_HEIGHT;
}

Vector2 RayGame_MouseWorld(void)
{
    Vector2 mouse = GetMousePosition();
    if (gameOptions.fillScreen) {
        mouse.x = mouse.x * (float)RAYGAME_CANVAS_WIDTH / (float)GetScreenWidth();
        mouse.y = mouse.y * (float)RAYGAME_CANVAS_HEIGHT / (float)GetScreenHeight();
        return mouse;
    }
    const float factor = zoom > 0.0f ? zoom : 1.0f;
    mouse.x = (mouse.x - (float)GetScreenWidth() * 0.5f) / factor + (float)RAYGAME_CANVAS_WIDTH * 0.5f;
    mouse.y = (mouse.y - (float)GetScreenHeight() * 0.5f) / factor + (float)RAYGAME_CANVAS_HEIGHT * 0.5f;
    return mouse;
}

void RayGame_SetFPS(int fps)
{
    if (fps < 0) fps = 10;
    else if (fps > 0 && fps < 10) fps = 10;

    targetFps = fps;
    gameOptions.fps = fps;

    if (!vsyncEnabled)
        SetTargetFPS(targetFps);
}

/* Taxa de atualizacao do monitor da janela (Hz). Windows: EnumDisplaySettings; senao o raylib. */
static int RayGame_RefreshHz(void)
{
    int hz = Display_RefreshHz(GetWindowHandle());
    if (hz <= 0) hz = GetMonitorRefreshRate(GetCurrentMonitor());
    return hz > 0 ? hz : 60;
}

/* V-SYNC: liga/desliga o swap interval da janela (o FLAG_VSYNC_HINT so vale na criacao).
   Com V-SYNC, o FPS tambem fica limitado a taxa do monitor, mesmo se o driver ignorar o pedido. */
void RayGame_SetVsync(char on)
{
    vsyncEnabled = (on != 0);
    gameOptions.vsync = vsyncEnabled;

    Display_SetSwapInterval(vsyncEnabled);

    if (vsyncEnabled)
        SetTargetFPS(RayGame_RefreshHz());
    else
        SetTargetFPS(targetFps);
}

int RayGame_GetTargetFPS(void)
{
    return targetFps;
}

/* ================================================================
   5. INICIALIZAÇÃO
   ================================================================ */

/* Logs do raylib (TraceLog) passam pelo Log.c. INFO/DEBUG/TRACE (texturas carregadas, subidas para
   a GPU, etc.) sao descartados: so avisos e erros entram no log. Fatal continua saindo do processo. */
static void RaylibTraceToLog(int logType, const char *text, va_list args)
{
    if (logType == LOG_INFO || logType == LOG_DEBUG || logType == LOG_TRACE) return;

    char buf[512];
    vsnprintf(buf, sizeof(buf), text, args);

    if (logType == LOG_FATAL) Log_Fatal(buf);   /* nao retorna */
    else                      Log_Info(buf);
}

void RayGame_Start(const char *gameTitle)
{
    SetTraceLogCallback(RaylibTraceToLog);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_WINDOW_RESIZABLE);

    InitWindow(RAYGAME_CANVAS_WIDTH, RAYGAME_CANVAS_HEIGHT, gameTitle);

    if (!IsWindowReady())
    {
        Log_Fatal("[ ERROR ] could not initialize the game window!");
        return;
    }

    SetExitKey(KEY_NULL);
    InitAudioDevice();
    audioReady = IsAudioDeviceReady();

    GameData_Start();
    gameDataReady = true;
    Jobs_Init(gameOptions.threadLimit);   // 0 = Auto (nucleos - 1)

    RayGame_SetVsync(gameOptions.vsync);
    if (!vsyncEnabled)
        RayGame_SetFPS(gameOptions.fps);

    stretchTarget = LoadRenderTexture(RAYGAME_CANVAS_WIDTH, RAYGAME_CANVAS_HEIGHT);
    stretchReady = (stretchTarget.id != 0);

    if (stretchReady)
        SetTextureFilter(stretchTarget.texture, TEXTURE_FILTER_BILINEAR);

    RayGame_UpdateViewport(true);
}

/* ================================================================
   6. LOOP PRINCIPAL
   ================================================================ */

void RayGame_GameLoop(void)
{
    if (game.scene == NULL)
    {
        Log_Fatal("[ ERROR ] game is not using any scene!");
        return;
    }

    while (!WindowShouldClose())
    {
        dt = GetFrameTime();
        if (dt < 0.0f) dt = 0.0f;
#if defined(FNF_DEBUG_STATS)
        const double tLoop = dbg.on ? GetTime() : 0.0;
#endif

        RayGame_UpdateViewport(false);
        RayGame_UpdateOverlay(dt);

        if (IsKeyPressed(KEY_F11))
        {
            ToggleBorderlessWindowed();
            RayGame_UpdateViewport(true);
        }

#if defined(FNF_DEBUG_STATS)
        if (IsKeyPressed(KEY_F3))
            dbg.on = !dbg.on;
#endif

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            RayGame_BumpMusicVolume(0.1f);

        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            RayGame_BumpMusicVolume(-0.1f);

        if (game.scene != NULL && RayGame_HadInput())
            RayScene_RequestRedraw(game.scene);

        if (game.scene != NULL && game.scene->functions.updateFunc != NULL)
            game.scene->functions.updateFunc(game.scene);

        if (playingMusic && musicLoaded)
            UpdateMusicStream(gameMusic);

        /* Frame parado: cena opt-in sem pedido de redesenho pula o draw e dorme.
           Nao pula com FPS visivel ou overlay F3 (eles mudam a cada frame). */
        {
            static float idleAge = 0.0f;
            const bool idleOk = game.scene != NULL && game.scene->staticWhenIdle
                && !game.scene->redrawRequested && idleAge < RAYGAME_IDLE_REDRAW_S
                && !gameOptions.showFps;
#if defined(FNF_DEBUG_STATS)
            const bool idleOkDbg = idleOk && !dbg.on;
#else
            const bool idleOkDbg = idleOk;
#endif
            if (idleOkDbg)
            {
                idleAge += dt;
                PollInputEvents();     /* EndDrawing nao roda aqui: precisa ler o input na mao */
                WaitTime(RAYGAME_IDLE_SLEEP_S);
                continue;
            }
            idleAge = 0.0f;
        }

        if (game.scene == NULL)
        {
            BeginDrawing();
            ClearBackground(BLACK);
            RayGame_DrawOverlay();
            EndDrawing();
            continue;
        }

#if defined(FNF_DEBUG_STATS)
        const double tDraw0 = dbg.on ? GetTime() : 0.0;
        if (dbg.on) dbg.upd = (tDraw0 - tLoop) * 1000.0;
#else
        const double tDraw0 = 0.0;
#endif

        game.scene->redrawRequested = false;   /* este frame vai ser desenhado */

        /* Sempre desenha no canvas 1280x720 (render target) e so depois escala para a janela:
           cameras, HUD e textos ficam no mesmo espaco, com ou sem Stretch. */
        if (stretchReady)
        {
            BeginTextureMode(stretchTarget);
            ClearBackground(BLACK);

            if (game.scene->functions.drawFunc != NULL)
                game.scene->functions.drawFunc(game.scene);

            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            Rectangle dstRect = { 0.0f, 0.0f, (float)width, (float)height };
            if (!gameOptions.fillScreen)
            {
                /* canvas centralizado com o mesmo zoom do letterbox */
                const float cw = (float)RAYGAME_CANVAS_WIDTH * zoom;
                const float ch = (float)RAYGAME_CANVAS_HEIGHT * zoom;
                dstRect = (Rectangle){ ((float)width - cw) * 0.5f, ((float)height - ch) * 0.5f, cw, ch };
            }

            const int fxDrawn = ShaderFx_BeginDraw((float)RAYGAME_CANVAS_WIDTH, (float)RAYGAME_CANVAS_HEIGHT);
            DrawTexturePro(
                stretchTarget.texture,
                (Rectangle){ 0.0f, 0.0f, (float)RAYGAME_CANVAS_WIDTH, -(float)RAYGAME_CANVAS_HEIGHT },
                dstRect,
                (Vector2){0.0f, 0.0f},
                0.0f,
                WHITE
            );
            if (fxDrawn) ShaderFx_EndDraw();

            RayGame_DrawOverlay();
            RayGame_EndFrame(tDraw0);
        }
        else
        {
            BeginDrawing();
            ClearBackground(BLACK);

            if (game.scene->functions.drawFunc != NULL)
                game.scene->functions.drawFunc(game.scene);

            {
                const int contentHeight = (int)(RAYGAME_CANVAS_HEIGHT * zoom);
                const int contentWidth  = (int)(RAYGAME_CANVAS_WIDTH * zoom);
                const int barH = (height - contentHeight) / 2;
                const int barW = (width - contentWidth) / 2;

                if (barH > 0)
                {
                    DrawRectangle(0, 0, width, barH, BLACK);
                    DrawRectangle(0, height - barH, width, barH, BLACK);
                }

                if (barW > 0)
                {
                    DrawRectangle(0, 0, barW, height, BLACK);
                    DrawRectangle(width - barW, 0, barW, height, BLACK);
                }
            }

            RayGame_DrawOverlay();
            RayGame_EndFrame(tDraw0);
        }
    }
}

/* ================================================================
   7. DELTA TIME / LIMPEZA
   ================================================================ */

float RayGame_DeltaTime(void)
{
    return dt;
}

void RayGame_ClearScene(void)
{
    if (game.scene != NULL)
    {
        Render_StopCamera();
        RayScene_Destroy(game.scene);
        game.scene = NULL;
    }
}

void RayGame_Destroy(void)
{
    RayGame_ClearScene();
    ShaderFx_Unload();

    /* Workers param ANTES de fechar a janela (CloseWindow/WM_DESTROY); senao eles seguem
     * segurando arquivos e a pasta nao pode ser apagada. Espera no maximo ~2,5 s; se algum
     * worker nao acabar (ex.: recompressao de PNG enorme), salva e encerra o processo na hora. */
    {
        const double t0 = GetTime();
        char clean = 0;

        ModImport_Cancel();
        while (GetTime() - t0 < 2.5)
        {
            const char importDone = (char)(!ModImport_Busy() ||
                                           ModImport_Poll(NULL, NULL, 0, NULL, NULL));
            Jobs_Poll();
            if (importDone && Jobs_Pending() == 0)
            {
                clean = 1;
                break;
            }
            Thread_SleepMs(5);
        }

        if (!clean)
        {
            Log_Info("[ INFO ] workers did not stop in time, exiting");
            if (gameDataReady)
            {
                GameData_Stop();
                gameDataReady = false;
            }
            fflush(stdout);
            fflush(stderr);
            _Exit(0);
        }
        ModImport_Shutdown();
        Jobs_Shutdown();
    }

    if (musicLoaded && audioReady)
    {
        StopMusicStream(gameMusic);
        UnloadMusicStream(gameMusic);
        gameMusic = (Music){0};
        musicLoaded = false;
        playingMusic = false;
    }

    if (fpsLabelReady)
    {
        UnloadRenderTexture(fpsLabel);
        fpsLabel = (RenderTexture2D){0};
        fpsLabelReady = false;
    }

    if (stretchReady)
    {
        UnloadRenderTexture(stretchTarget);
        stretchTarget = (RenderTexture2D){0};
        stretchReady = false;
    }

    if (gameDataReady)
    {
        GameData_Stop();
        gameDataReady = false;
    }

    if (audioReady)
    {
        CloseAudioDevice();
        audioReady = false;
    }

    if (IsWindowReady())
        CloseWindow();
}