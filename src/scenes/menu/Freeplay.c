/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * Freeplay.c - lista de musicas e painel da musica selecionada.
 * A busca do icone fica em FreeplayIcon.c e so roda quando a selecao muda.
 */

#include "scenes/AllScenes.h"
#include "scenes/SongList.h"
#include "scenes/menu/FreeplayIcon.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

#define LIST_STEP 92.0f
#define LIST_VISIBLE_RADIUS 6.0f   // linhas desenhadas para cada lado da selecao
#define LIST_CULL_TOP -90.0f
#define LIST_SCROLL_SPEED 10.0f

#define PANEL_X 850.0f
#define PANEL_W 360.0f
#define ICON_X 920.0f
#define ICON_Y 112.0f
#define ICON_SIZE 224.0f

static const Color PANEL_BG = {0, 0, 0, 125};
static const Color PANEL_BORDER = {110, 105, 155, 170};
static const Color PANEL_LABEL = {190, 185, 220, 255};
static const Color ICON_FRAME_BG = {18, 18, 28, 255};
static const Color ICON_FRAME_BORDER = {190, 165, 255, 220};
static const Color ICON_EMPTY_BG = {38, 38, 52, 255};
static const Color ICON_EMPTY_TEXT = {150, 145, 170, 255};
static const Color OPPONENT_LABEL = {175, 170, 205, 255};
static const Color OPPONENT_NAME = {255, 220, 100, 255};
static const Color ROW_BG = {20, 18, 38, 170};
static const Color ROW_ACCENT = {255, 215, 90, 255};
static const Color ROW_TEXT_IDLE = {205, 205, 220, 165};
static const Color ROW_ARROW = {255, 220, 90, 255};

static SongEntry* songs;
static int songCount;
static int selectedSong;
static float listOffset;

static RayGraphicObject bg;
static Camera2D cam;

/* Layout do painel: so muda quando a selecao muda, entao nao mede texto por frame. */
static float titleX;
static char counterText[48];

/* ------------------------------------------------------------------ */
/* Selecao                                                             */
/* ------------------------------------------------------------------ */

/* Carrega o icone e refaz o texto do painel da musica selecionada. */
static void SelectSong(void) {
    FreeplayIcon_Load(songs + selectedSong);

    const Vector2 size = MeasureTextEx(mainFont, songs[selectedSong].name, 34.0f, 2.0f);
    titleX = fmaxf(PANEL_X + (PANEL_W - size.x) * 0.5f, PANEL_X + 16.0f);

    snprintf(counterText, sizeof(counterText), "TRACK %d / %d", selectedSong + 1, songCount);
}

static void PlaySelected(void) {
    const char* dir = songs[selectedSong].dir;

    // O destino vale tanto para o PlayState direto quanto via LoadingState.
    PlayState_SetSongDir(dir);
    RayGame_ToggleMusic(0);

    if (SongList_IsHeavy(dir)) {
        LoadingState_SetSongDir(dir);
        LoadingState_SetScene();
    } else {
        PlayState_SetScene();
    }
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void Freeplay_Create([[maybe_unused]] RayScene* scene) {
    FreeplayIcon_Unload();
    SongList_Free(songs);
    songs = NULL;

    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");

    cam = (Camera2D) {
        .target = {SCREEN_W / 2, SCREEN_H / 2},
        .zoom = 1.0f
    };

    songCount = SongList_Scan(&songs);
    if (songCount < 0) songCount = 0;

    selectedSong = 0;
    listOffset = 0.0f;

    if (songCount > 0) SelectSong();
}

static void Freeplay_Update([[maybe_unused]] RayScene* scene) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        MenuState_SetScene();
        return;
    }

    if (songCount <= 0) return;

    const int previous = selectedSong;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        if (++selectedSong >= songCount) selectedSong = 0;
    }
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        if (--selectedSong < 0) selectedSong = songCount - 1;
    }

    if (selectedSong != previous)
        SelectSong();

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        PlaySelected();
        return;
    }

    // Rolagem independente de framerate.
    const float factor = 1.0f - expf(-LIST_SCROLL_SPEED * fmaxf(RayGame_DeltaTime(), 0.0f));
    listOffset += ((float)selectedSong - listOffset) * factor;
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

/* Recorte quadrado no centro da textura, mantendo a proporcao. */
static Rectangle SquareCrop(Texture2D texture) {
    const float w = (float)texture.width;
    const float h = (float)texture.height;
    Rectangle source = {0.0f, 0.0f, w, h};

    if (w > h) {
        source.x = (w - h) * 0.5f;
        source.width = h;
    } else if (h > w) {
        source.y = (h - w) * 0.5f;
        source.height = w;
    }
    return source;
}

static void DrawIconFrame(void) {
    const int frameX = (int)(ICON_X - 8.0f), frameY = (int)(ICON_Y - 8.0f);
    const int frameSize = (int)(ICON_SIZE + 16.0f);

    DrawRectangle(frameX, frameY, frameSize, frameSize, ICON_FRAME_BG);
    DrawRectangleLines(frameX, frameY, frameSize, frameSize, ICON_FRAME_BORDER);

    if (FreeplayIcon_IsLoaded()) {
        const Texture2D texture = FreeplayIcon_Texture();
        DrawTexturePro(texture, SquareCrop(texture), (Rectangle) {ICON_X, ICON_Y, ICON_SIZE, ICON_SIZE},
                       (Vector2) {0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }

    DrawRectangle((int)ICON_X, (int)ICON_Y, (int)ICON_SIZE, (int)ICON_SIZE, ICON_EMPTY_BG);
    static const char* const missing = "NO ICON";
    const Vector2 size = MeasureTextEx(mainFont, missing, 25.0f, 2.0f);
    DrawTextEx(mainFont, missing,
               (Vector2) {ICON_X + (ICON_SIZE - size.x) * 0.5f, ICON_Y + (ICON_SIZE - size.y) * 0.5f},
               25.0f, 2.0f, ICON_EMPTY_TEXT);
}

static void DrawSelectedPanel(void) {
    DrawRectangle((int)PANEL_X, 62, (int)PANEL_W, 590, PANEL_BG);
    DrawRectangleLines((int)PANEL_X, 62, (int)PANEL_W, 590, PANEL_BORDER);

    DrawTextEx(mainFont, "SELECTED TRACK", (Vector2) {PANEL_X + 24.0f, 82.0f}, 21.0f, 2.0f, PANEL_LABEL);

    DrawIconFrame();

    const char* title = songs[selectedSong].name;
    DrawTextEx(mainFont, title, (Vector2) {titleX + 2.0f, 375.0f}, 34.0f, 2.0f, (Color) {0, 0, 0, 210});
    DrawTextEx(mainFont, title, (Vector2) {titleX, 373.0f}, 34.0f, 2.0f, WHITE);

    DrawTextEx(mainFont, "OPPONENT", (Vector2) {PANEL_X + 24.0f, 435.0f}, 19.0f, 2.0f, OPPONENT_LABEL);
    DrawTextEx(mainFont, FreeplayIcon_Opponent(), (Vector2) {PANEL_X + 24.0f, 462.0f}, 25.0f, 2.0f, OPPONENT_NAME);

    DrawTextEx(mainFont, counterText, (Vector2) {PANEL_X + 24.0f, 555.0f}, 19.0f, 1.5f, PANEL_LABEL);
}

static void DrawSongRow(int i) {
    const float y = 360.0f + ((float)i - listOffset) * LIST_STEP;
    if (y < LIST_CULL_TOP || y > SCREEN_H)
        return; // fora da tela

    const bool isSelected = i == selectedSong;
    const float x = 145.0f - fabsf((float)i - listOffset) * 24.0f;
    const char* name = songs[i].name;

    if (isSelected) {
        DrawRectangle(105, (int)(y - 8.0f), 675, 78, ROW_BG);
        DrawRectangle(105, (int)(y - 8.0f), 5, 78, ROW_ACCENT);
    }

    const Color shadow = {0, 0, 0, isSelected ? 230 : 120};
    DrawTextEx(mainFont, name, (Vector2) {x + 4.0f, y + 4.0f}, 52.0f, 3.0f, shadow);
    DrawTextEx(mainFont, name, (Vector2) {x, y}, 52.0f, 3.0f, isSelected ? WHITE : ROW_TEXT_IDLE);

    if (isSelected)
        DrawTextEx(mainFont, ">", (Vector2) {x - 45.0f, y - 1.0f}, 55.0f, 3.0f, ROW_ARROW);
}

static void Freeplay_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);

    if (songCount <= 0) {
        DrawTextEx(mainFont, "NO SONGS FOUND", (Vector2) {150.0f, 330.0f}, 42.0f, 3.0f, WHITE);
        DrawTextEx(mainFont, "Add songs to assets/songs/ or assets/mods/<mod>/songs/",
                   (Vector2) {150.0f, 390.0f}, 21.0f, 1.5f, (Color) {210, 210, 220, 255});
        Render_StopCamera();
        return;
    }

    // So as linhas perto da selecao; as demais nem sao percorridas.
    const int first = (int)fmaxf(0.0f, floorf(listOffset - LIST_VISIBLE_RADIUS));
    const int last = (int)fminf((float)(songCount - 1), ceilf(listOffset + LIST_VISIBLE_RADIUS));
    for (int i = first; i <= last; ++i)
        DrawSongRow(i);

    DrawSelectedPanel();

    DrawTextEx(mainFont, "UP / DOWN  SELECT TRACK", (Vector2) {105.0f, 660.0f}, 19.0f, 1.5f, (Color) {245, 220, 120, 255});
    DrawTextEx(mainFont, "ENTER PLAY     ESC BACK", (Vector2) {850.0f, 675.0f}, 17.0f, 1.0f, (Color) {220, 215, 235, 230});

    Render_StopCamera();
}

static void Freeplay_Destroy([[maybe_unused]] RayScene* scene) {
    FreeplayIcon_Unload();
    if (bg.image.id != 0) UnloadTexture(bg.image);
    bg.image = (Texture2D) {0};

    SongList_Free(songs);
    songs = NULL;
    songCount = 0;
    selectedSong = 0;
    listOffset = 0.0f;
}

static RayScene scene;
Scene_MakeSceneCode(
    scene,
    Freeplay_SetScene,
    Freeplay_Create,
    Freeplay_Update,
    Freeplay_Draw,
    Freeplay_Destroy
);
