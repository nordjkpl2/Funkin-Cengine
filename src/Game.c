/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * RayGame.c - Estado global, áudio, janela e loop principal.
 */

#include "std.h"
#include "Game.h"
#include "GameData.h"
#include "Log.h"

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

static inline void RayGame_UpdateOverlay(float frameDelta)
{
    if (volOsdTimer > 0.0f)
    {
        volOsdTimer -= frameDelta;
        if (volOsdTimer < 0.0f)
            volOsdTimer = 0.0f;
    }
}

static inline void RayGame_DrawOverlay(void)
{
    if (gameOptions.showFps)
        DrawFPS(10, 3);

    if (volOsdTimer > 0.0f)
        DrawText(volumeText, 10, 26, 20, WHITE);
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
    return gameOptions.fillScreen ? RAYGAME_CANVAS_WIDTH : width;
}

int RayGame_CanvasHeight(void)
{
    return gameOptions.fillScreen ? RAYGAME_CANVAS_HEIGHT : height;
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

void RayGame_SetVsync(char on)
{
    vsyncEnabled = (on != 0);
    gameOptions.vsync = vsyncEnabled;

    if (vsyncEnabled)
    {
        SetWindowState(FLAG_VSYNC_HINT);
        SetTargetFPS(0);
    }
    else
    {
        ClearWindowState(FLAG_VSYNC_HINT);
        SetTargetFPS(targetFps);
    }
}

int RayGame_GetTargetFPS(void)
{
    return targetFps;
}

/* ================================================================
   5. INICIALIZAÇÃO
   ================================================================ */

void RayGame_Start(const char *gameTitle)
{
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);

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

        RayGame_UpdateViewport(false);
        RayGame_UpdateOverlay(dt);

        if (IsKeyPressed(KEY_F11))
        {
            ToggleBorderlessWindowed();
            RayGame_UpdateViewport(true);
        }

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            RayGame_BumpMusicVolume(0.1f);

        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            RayGame_BumpMusicVolume(-0.1f);

        if (game.scene != NULL && game.scene->functions.updateFunc != NULL)
            game.scene->functions.updateFunc(game.scene);

        if (playingMusic && musicLoaded)
            UpdateMusicStream(gameMusic);

        if (game.scene == NULL)
        {
            BeginDrawing();
            ClearBackground(BLACK);
            RayGame_DrawOverlay();
            EndDrawing();
            continue;
        }

        if (gameOptions.fillScreen && stretchReady)
        {
            BeginTextureMode(stretchTarget);
            ClearBackground(BLACK);

            if (game.scene->functions.drawFunc != NULL)
                game.scene->functions.drawFunc(game.scene);

            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                stretchTarget.texture,
                (Rectangle){ 0.0f, 0.0f, (float)RAYGAME_CANVAS_WIDTH, -(float)RAYGAME_CANVAS_HEIGHT },
                (Rectangle){ 0.0f, 0.0f, (float)width, (float)height },
                (Vector2){0.0f, 0.0f},
                0.0f,
                WHITE
            );

            RayGame_DrawOverlay();
            EndDrawing();
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
            EndDrawing();
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

    if (musicLoaded && audioReady)
    {
        StopMusicStream(gameMusic);
        UnloadMusicStream(gameMusic);
        gameMusic = (Music){0};
        musicLoaded = false;
        playingMusic = false;
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