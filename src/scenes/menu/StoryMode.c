/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * StoryMode.c - selecao de weeks do Story Mode.
 *
 * A busca da capa fica em StoryCover.c. Textos que dependem so da week (titulo
 * cortado, contagem de musicas, lista de faixas) sao montados quando a lista ou a
 * selecao muda, nunca no Draw.
 */

#include "scenes/AllScenes.h"
#include "data/SongList.h"
#include "scenes/menu/StoryCover.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"
#include "data/Week.h"

#define SCREEN_W 1280
#define SCREEN_H 720

#define LIST_X 58.0f
#define LIST_Y 164.0f
#define LIST_W 610.0f
#define LIST_BOTTOM 642.0f
#define ROW_H 72.0f
#define ROW_BOX_H 62.0f
#define VISIBLE_MARGIN 100.0f
#define TITLE_MAX_W (LIST_W - 92.0f)
#define ROW_FONT_IDLE 26.0f
#define ROW_FONT_SELECTED 29.0f

#define COVER_SIZE 224.0f
#define COVER_X 910.0f
#define COVER_Y 124.0f

#define DETAILS_X 760.0f
#define DETAILS_Y 430.0f
#define DETAILS_W 460.0f
#define DETAILS_H 278.0f
#define DETAIL_COLUMNS 3
#define DETAIL_COLUMN_W 148.0f
#define DETAIL_ROW_H 13.7f
#define DETAIL_LINE_MAX 24

#define TEXT_MAX 192
#define STATUS_SECONDS 3.0f
#define MAX_DISPLAY_SONGS 50
#define SCROLL_SPEED 12.0f
#define DISC_SPEED 0.65f
#define TWO_PI 6.28318530718f

static const Color COLOR_PANEL = {12, 15, 27, 218};
static const Color COLOR_PANEL_ALT = {22, 27, 44, 230};
static const Color COLOR_BORDER = {90, 105, 145, 150};
static const Color COLOR_TEXT = {238, 241, 255, 255};
static const Color COLOR_MUTED = {155, 165, 190, 255};
static const Color COLOR_ACCENT = {255, 205, 92, 255};
static const Color COLOR_GREEN = {100, 225, 155, 255};
static const Color COLOR_DANGER = {255, 105, 115, 255};

/* Textos prontos de cada week na lista. */
typedef struct {
    char idleTitle[TEXT_MAX];     // cortado para caber, fonte normal
    char selectedTitle[TEXT_MAX]; // cortado para caber, fonte da selecionada
    char count[16];               // "3 SONGS"
} WeekRowText;

static WeekEntry* weeks;
static WeekRowText* rowTexts;
static int weekCount;
static int selected;
static float listOffset;

static Texture2D backgroundTexture;
static Camera2D storyCamera;
static float discAngle;

static char deleteConfirm;
static char deletePrompt[TEXT_MAX];

static char statusText[128];
static float statusWidth;
static float statusTimer;

/* Painel de detalhes da week selecionada. */
static float detailsTitleX;
static char detailsCount[64];
static char detailsLastScore[48];
static char detailsLines[MAX_DISPLAY_SONGS][DETAIL_LINE_MAX];
static int detailsLineCount;
static int detailsRowsPerColumn = 1;
static bool detailsHidden; // week com mais faixas do que o painel mostra

/* ------------------------------------------------------------------ */
/* Textos em cache                                                     */
/* ------------------------------------------------------------------ */

static const char* WeekTitle(const WeekEntry* week) {
    return week->title[0] ? week->title : "Untitled Week";
}

static void SetStatus(const char* message) {
    snprintf(statusText, sizeof(statusText), "%s", message);
    statusWidth = MeasureTextEx(mainFont, statusText, 18.0f, 1.0f).x + 36.0f;
    statusTimer = STATUS_SECONDS;
}

/* Copia o titulo e corta com "..." ate caber em TITLE_MAX_W. */
static void FitTitle(const char* title, float fontSize, char out[TEXT_MAX]) {
    size_t length = strlen(title);
    if (length >= TEXT_MAX) length = TEXT_MAX - 1;
    memcpy(out, title, length);
    out[length] = '\0';

    if (MeasureTextEx(mainFont, out, fontSize, 1.5f).x <= TITLE_MAX_W)
        return;

    while (length > 4 && MeasureTextEx(mainFont, out, fontSize, 1.5f).x > TITLE_MAX_W)
        out[--length] = '\0';

    if (length > 3)
        memcpy(out + length - 3, "...", 3);
}

static void RowTexts_Build(void) {
    for (int i = 0; i < weekCount; i++) {
        const char* title = WeekTitle(weeks + i);
        FitTitle(title, ROW_FONT_IDLE, rowTexts[i].idleTitle);
        FitTitle(title, ROW_FONT_SELECTED, rowTexts[i].selectedTitle);
        snprintf(rowTexts[i].count, sizeof(rowTexts[i].count), "%d SONGS", weeks[i].songCount);
    }
}

/* Refaz o painel de detalhes da week selecionada. */
static void Details_Build(void) {
    if (selected < 0 || selected >= weekCount) return;

    const WeekEntry* week = weeks + selected;
    const int shown = week->songCount > MAX_DISPLAY_SONGS ? MAX_DISPLAY_SONGS
                    : week->songCount < 0 ? 0 : week->songCount;

    const Vector2 titleSize = MeasureTextEx(mainFont, WeekTitle(week), 36.0f, 2.0f);
    detailsTitleX = fmaxf(1210.0f - titleSize.x, DETAILS_X);

    detailsHidden = week->songCount > MAX_DISPLAY_SONGS;
    if (detailsHidden)
        snprintf(detailsCount, sizeof(detailsCount), "SHOWING %d OF %d TRACKS", shown, week->songCount);
    else
        snprintf(detailsCount, sizeof(detailsCount), "%d TRACKS", week->songCount);

    detailsLineCount = shown;
    detailsRowsPerColumn = (shown + DETAIL_COLUMNS - 1) / DETAIL_COLUMNS;
    if (detailsRowsPerColumn < 1) detailsRowsPerColumn = 1;

    for (int i = 0; i < shown; i++) {
        const char* name = week->songs[i].song[0] ? week->songs[i].song : "Unknown";
        snprintf(detailsLines[i], DETAIL_LINE_MAX, "%02d  %.17s", i + 1, name);
    }

    if (WeekRun_LastTotal() > 0)
        snprintf(detailsLastScore, sizeof(detailsLastScore), "LAST SCORE  %d", WeekRun_LastTotal());
    else
        detailsLastScore[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Lista de weeks e selecao                                            */
/* ------------------------------------------------------------------ */

static void SelectWeek(int index) {
    selected = index;
    StoryCover_Load(selected, weeks + selected);
    Details_Build();
}

static void RefreshWeekList(void) {
    WeekList_Free(weeks);
    weeks = NULL;
    free(rowTexts);
    rowTexts = NULL;

    weekCount = WeekList_Scan(&weeks);
    if (weekCount < 0) weekCount = 0;

    StoryCover_Reset(weekCount);

    if (selected >= weekCount) selected = weekCount - 1;
    if (selected < 0) selected = 0;
    if (listOffset > (float)selected) listOffset = (float)selected;

    if (weekCount <= 0) return;

    rowTexts = (WeekRowText*)calloc((size_t)weekCount, sizeof(*rowTexts));
    if (!rowTexts) {
        // Sem memoria: mostra como se nao houvesse weeks, em vez de quebrar o Draw.
        WeekList_Free(weeks);
        weeks = NULL;
        weekCount = 0;
        return;
    }

    RowTexts_Build();
    SelectWeek(selected);
}

static void StartSelectedWeek(void) {
    if (!weeks || selected < 0 || selected >= weekCount) return;

    WeekRun_Start(weeks + selected, 1);
    const char* songDir = WeekRun_Dir();

    if (!songDir || !songDir[0]) {
        const char* missing = WeekRun_Missing();
        char message[128];

        if (missing && missing[0])
            snprintf(message, sizeof(message), "MISSING SONG: %.96s", missing);
        else
            snprintf(message, sizeof(message), "THIS WEEK HAS NO PLAYABLE SONGS");

        SetStatus(message);
        WeekRun_Stop();
        return;
    }

    RayGame_ToggleMusic(0);
    PlayState_SetSongDir(songDir);
    PlayState_SetReturnWeek(1);
    PlayState_SetCharsOverride(WeekRun_Char());
    PlayState_SetDiff(WeekRun_DiffSuffix());

    if (SongList_IsHeavy(songDir)) {
        LoadingState_SetSongDir(songDir);
        LoadingState_SetScene();
    } else {
        PlayState_SetScene();
    }
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void StoryMode_Create([[maybe_unused]] RayScene* scene) {
    backgroundTexture = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");

    storyCamera = (Camera2D) {
        .target = {SCREEN_W * 0.5f, SCREEN_H * 0.5f},
        .zoom = 1.0f
    };

    selected = 0;
    listOffset = 0.0f;
    deleteConfirm = 0;
    discAngle = 0.0f;
    statusText[0] = '\0';
    statusTimer = 0.0f;

    RefreshWeekList();

    if (WeekRun_Missing()[0]) {
        char message[128];
        snprintf(message, sizeof(message), "SKIPPED (MISSING): %.96s", WeekRun_Missing());
        SetStatus(message);
    }
}

/* Confirmacao de apagar a week: Y confirma; N, ESC ou BACKSPACE cancelam. */
static void UpdateDeleteConfirm(void) {
    if (IsKeyPressed(KEY_Y)) {
        const int deleted = weeks && selected >= 0 && selected < weekCount
                          ? Week_Delete(weeks + selected) : 0;
        SetStatus(deleted ? "WEEK DELETED" : "COULD NOT DELETE WEEK");
        deleteConfirm = 0;
        RefreshWeekList();
    }
    else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        deleteConfirm = 0;
    }
}

static void StoryMode_Update([[maybe_unused]] RayScene* scene) {
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);

    if (statusTimer > 0.0f) {
        statusTimer -= dt;
        if (statusTimer <= 0.0f) {
            statusTimer = 0.0f;
            statusText[0] = '\0';
        }
    }

    discAngle += dt * DISC_SPEED;
    if (discAngle >= TWO_PI)
        discAngle -= TWO_PI;

    if (deleteConfirm) {
        UpdateDeleteConfirm();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        MenuState_SetScene();
        return;
    }

    if (weekCount <= 0)
        return;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        SelectWeek((selected + 1) % weekCount);
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        SelectWeek((selected - 1 + weekCount) % weekCount);

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_SLASH) || IsKeyPressed(KEY_KP_DIVIDE)) {
        snprintf(deletePrompt, sizeof(deletePrompt), "DELETE %.80s?", weeks[selected].title);
        deleteConfirm = 1;
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        StartSelectedWeek();
        return;
    }

    // Suavizacao exponencial: sem overshoot e igual em qualquer FPS.
    listOffset += ((float)selected - listOffset) * (1.0f - expf(-SCROLL_SPEED * dt));
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

static void DrawPanel(Rectangle rect, Color fill, Color border) {
    DrawRectangleRounded(rect, 0.045f, 8, fill);
    DrawRectangleLinesEx(rect, 1.0f, border);
}

static void DrawCover(void) {
    const Rectangle card = {COVER_X - 18.0f, COVER_Y - 18.0f, COVER_SIZE + 36.0f, COVER_SIZE + 36.0f};
    const Rectangle destination = {COVER_X, COVER_Y, COVER_SIZE, COVER_SIZE};

    DrawPanel(card, COLOR_PANEL_ALT, COLOR_BORDER);

    if (StoryCover_IsLoaded()) {
        const Texture2D texture = StoryCover_Texture();
        Rectangle source = {0.0f, 0.0f, (float)texture.width, (float)texture.height};

        // Recorte quadrado no centro, sem esticar a arte.
        if (source.width > source.height) {
            source.x = (source.width - source.height) * 0.5f;
            source.width = source.height;
        } else if (source.height > source.width) {
            source.y = (source.height - source.width) * 0.5f;
            source.height = source.width;
        }

        DrawTexturePro(texture, source, destination, VECTOR_ZERO, 0.0f, WHITE);
    } else {
        DrawRectangleRec(destination, (Color) {28, 33, 52, 255});
        DrawTextEx(mainFont, "NO WEEK ART", (Vector2) {COVER_X + 43.0f, COVER_Y + 98.0f}, 19.0f, 1.0f, COLOR_MUTED);
    }

    DrawRectangleLinesEx(destination, 3.0f, (Color) {235, 220, 180, 230});
}

/* Vinil pequeno abaixo da capa. */
static void DrawDisc(void) {
    const Vector2 center = {COVER_X + COVER_SIZE * 0.5f, 390.0f};
    const float radius = 29.0f;
    const Vector2 marker = {center.x + 24.0f * cosf(discAngle), center.y + 24.0f * sinf(discAngle)};

    DrawCircleV(center, radius + 4.0f, (Color) {4, 6, 13, 150});
    DrawCircleV(center, radius, (Color) {19, 22, 35, 255});
    DrawCircleLines((int)center.x, (int)center.y, radius - 2.0f, (Color) {105, 117, 150, 210});
    DrawCircleLines((int)center.x, (int)center.y, 20.0f, (Color) {62, 72, 103, 190});
    DrawCircleLines((int)center.x, (int)center.y, 12.0f, (Color) {62, 72, 103, 170});
    DrawCircleV(center, 7.0f, (Color) {104, 79, 153, 255});
    DrawCircleV(center, 2.5f, (Color) {231, 232, 242, 255});
    DrawLineEx(center, marker, 1.5f, (Color) {255, 218, 139, 210});
    DrawCircleV(marker, 2.0f, COLOR_ACCENT);
}

static void DrawWeekRow(int i) {
    const float y = LIST_Y + ((float)i - listOffset) * ROW_H;
    if (y + ROW_BOX_H < LIST_Y - VISIBLE_MARGIN || y > LIST_BOTTOM + VISIBLE_MARGIN)
        return;

    const Rectangle row = {LIST_X, y, LIST_W, ROW_BOX_H};
    const bool isSelected = i == selected;

    if (isSelected) {
        DrawPanel(row, (Color) {59, 49, 75, 242}, (Color) {255, 205, 92, 220});
        DrawRectangle((int)row.x, (int)row.y + 8, 4, 46, COLOR_ACCENT);
    } else {
        DrawRectangleRounded(row, 0.08f, 6, (Color) {12, 16, 29, 155});
        DrawRectangleLinesEx(row, 1.0f, (Color) {60, 72, 100, 90});
    }

    const WeekRowText* text = rowTexts + i;
    DrawTextEx(mainFont, isSelected ? text->selectedTitle : text->idleTitle,
               (Vector2) {row.x + 25.0f, row.y + 13.0f},
               isSelected ? ROW_FONT_SELECTED : ROW_FONT_IDLE, 1.5f,
               isSelected ? COLOR_TEXT : COLOR_MUTED);

    DrawTextEx(mainFont, text->count, (Vector2) {row.x + LIST_W - 112.0f, row.y + 22.0f},
               13.0f, 1.0f, isSelected ? COLOR_ACCENT : COLOR_MUTED);
}

static void DrawWeekList(void) {
    int first = (int)floorf(listOffset - VISIBLE_MARGIN / ROW_H) - 1;
    int last = (int)ceilf(listOffset + (LIST_BOTTOM - LIST_Y + VISIBLE_MARGIN) / ROW_H) + 1;
    if (first < 0) first = 0;
    if (last >= weekCount) last = weekCount - 1;

    for (int i = first; i <= last; ++i)
        DrawWeekRow(i);
}

static void DrawWeekDetails(void) {
    const float firstX = DETAILS_X + 18.0f;
    const float firstY = DETAILS_Y + 35.0f;

    const char* title = WeekTitle(weeks + selected);
    DrawTextEx(mainFont, title, (Vector2) {detailsTitleX + 2.0f, 48.0f}, 36.0f, 2.0f, (Color) {0, 0, 0, 180});
    DrawTextEx(mainFont, title, (Vector2) {detailsTitleX, 46.0f}, 36.0f, 2.0f, COLOR_TEXT);

    DrawPanel((Rectangle) {DETAILS_X, DETAILS_Y, DETAILS_W, DETAILS_H}, COLOR_PANEL, COLOR_BORDER);
    DrawTextEx(mainFont, detailsCount, (Vector2) {firstX, DETAILS_Y + 10.0f}, 14.0f, 1.0f, COLOR_ACCENT);

    // Tres colunas compactas: cabem ate 50 faixas dentro do painel.
    for (int i = 0; i < detailsLineCount; ++i) {
        const int column = i / detailsRowsPerColumn;
        const int row = i % detailsRowsPerColumn;
        if (column >= DETAIL_COLUMNS) break;

        DrawTextEx(mainFont, detailsLines[i],
                   (Vector2) {firstX + (float)column * DETAIL_COLUMN_W, firstY + (float)row * DETAIL_ROW_H},
                   12.5f, 1.0f, COLOR_TEXT);
    }

    if (detailsHidden)
        DrawTextEx(mainFont, "Additional tracks hidden (limit: 50)",
                   (Vector2) {firstX, DETAILS_Y + DETAILS_H - 17.0f}, 10.0f, 1.0f, COLOR_MUTED);

    if (detailsLastScore[0])
        DrawTextEx(mainFont, detailsLastScore, (Vector2) {DETAILS_X + DETAILS_W - 170.0f, DETAILS_Y + 9.0f},
                   12.0f, 1.0f, COLOR_GREEN);

    DrawDisc();
}

static void DrawDeleteModal(void) {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {0, 0, 0, 165});
    DrawPanel((Rectangle) {300.0f, 260.0f, 680.0f, 175.0f}, (Color) {27, 20, 34, 250}, COLOR_DANGER);
    DrawTextEx(mainFont, deletePrompt, (Vector2) {350.0f, 294.0f}, 30.0f, 1.5f, COLOR_TEXT);
    DrawTextEx(mainFont, "Y  CONFIRM", (Vector2) {390.0f, 365.0f}, 19.0f, 1.0f, COLOR_DANGER);
    DrawTextEx(mainFont, "N  CANCEL", (Vector2) {650.0f, 365.0f}, 19.0f, 1.0f, COLOR_GREEN);
}

static void StoryMode_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&storyCamera);

    ClearBackground((Color) {7, 9, 18, 255});
    if (backgroundTexture.id != 0) {
        DrawTexturePro(backgroundTexture,
                       (Rectangle) {0.0f, 0.0f, (float)backgroundTexture.width, (float)backgroundTexture.height},
                       (Rectangle) {0.0f, 0.0f, SCREEN_W, SCREEN_H},
                       VECTOR_ZERO, 0.0f, (Color) {115, 125, 160, 90});
    }
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {5, 7, 15, 145});

    DrawTextEx(mainFont, "STORY MODE", (Vector2) {58.0f, 44.0f}, 19.0f, 2.0f, COLOR_ACCENT);
    DrawTextEx(mainFont, "SELECT A WEEK", (Vector2) {58.0f, 72.0f}, 37.0f, 2.0f, COLOR_TEXT);
    DrawLineEx((Vector2) {58.0f, 126.0f}, (Vector2) {1218.0f, 126.0f}, 1.0f, COLOR_BORDER);

    if (weekCount <= 0) {
        DrawPanel((Rectangle) {250.0f, 245.0f, 780.0f, 190.0f}, COLOR_PANEL, COLOR_BORDER);
        DrawTextEx(mainFont, "NO WEEKS FOUND", (Vector2) {470.0f, 285.0f}, 34.0f, 2.0f, COLOR_TEXT);
        DrawTextEx(mainFont, "Add week JSON files to assets/weeks or assets/mods/<mod>/weeks.",
                   (Vector2) {315.0f, 350.0f}, 18.0f, 1.0f, COLOR_MUTED);
    } else {
        DrawWeekList();
        DrawCover();
        DrawWeekDetails();
    }

    if (deleteConfirm && weekCount > 0)
        DrawDeleteModal();

    if (statusTimer > 0.0f && statusText[0]) {
        DrawPanel((Rectangle) {58.0f, 646.0f, statusWidth, 36.0f}, (Color) {23, 30, 43, 235}, COLOR_BORDER);
        DrawTextEx(mainFont, statusText, (Vector2) {75.0f, 654.0f}, 18.0f, 1.0f, COLOR_ACCENT);
    }

    DrawTextEx(mainFont, "UP / DOWN  SELECT     ENTER  PLAY     DEL  DELETE     ESC  BACK",
               (Vector2) {58.0f, 690.0f}, 14.0f, 1.0f, COLOR_MUTED);

    Render_StopCamera();
}

static void StoryMode_Destroy([[maybe_unused]] RayScene* scene) {
    StoryCover_Free();

    if (backgroundTexture.id != 0)
        UnloadTexture(backgroundTexture);
    backgroundTexture = (Texture2D) {0};

    WeekList_Free(weeks);
    weeks = NULL;
    free(rowTexts);
    rowTexts = NULL;
    weekCount = 0;
    selected = 0;
}

static RayScene storyScene;
Scene_MakeSceneCode(
    storyScene,
    StoryMode_SetScene,
    StoryMode_Create,
    StoryMode_Update,
    StoryMode_Draw,
    StoryMode_Destroy
)
