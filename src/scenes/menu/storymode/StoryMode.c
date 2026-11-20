/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * StoryMode.c - selecao de weeks do Story Mode.
 *
 * A busca da capa fica em StoryCover.c. Textos que dependem so da week (titulo
 * cortado, contagem de musicas, lista de faixas) sao montados quando a lista ou a
 * selecao muda, nunca no Draw.
 */

#include "scenes/LoadingState.h"
#include "scenes/api/EndStartLoader.h"
#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayState.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/storymode/StoryMode.h"
#include "data/SongList.h"
#include "scenes/menu/storymode/StoryCover.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"
#include "data/Week.h"

#define SCREEN_W 1280
#define SCREEN_H 720

/* Layout: banner grande no topo; lista de weeks embaixo a esquerda; faixas a direita. */
#define BANNER_X -40.0f
#define BANNER_Y 0.0f
#define BANNER_W 1360.0f
#define BANNER_H 380.0f
#define DISC_CENTER_X 1236.0f
#define DISC_CENTER_Y 350.0f
#define DISC_RADIUS 24.0f
#define TITLE_RIGHT 1222.0f
#define TITLE_Y 618.0f

#define LIST_X 58.0f
#define LIST_Y 392.0f
#define LIST_W 610.0f
#define LIST_BOTTOM 644.0f
#define ROW_H 72.0f
#define ROW_BOX_H 62.0f
#define VISIBLE_MARGIN 100.0f
#define TITLE_MAX_W (LIST_W - 92.0f)
#define ROW_FONT_IDLE 26.0f
#define ROW_FONT_SELECTED 29.0f

#define DETAILS_X 660.0f
#define DETAILS_Y 392.0f
#define DETAILS_W 558.0f
#define DETAILS_H 220.0f
#define DETAIL_COLUMNS 2
#define DETAIL_COLUMN_W 250.0f
#define DETAIL_ROW_H 27.0f
#define DETAIL_FONT 20.0f
#define DETAIL_LIST_TOP 38.0f
#define DETAIL_LIST_H (DETAILS_H - DETAIL_LIST_TOP - 10.0f)
#define DETAIL_LINE_MAX 24

#define TEXT_MAX 192
#define STATUS_SECONDS 3.0f
#define MAX_DISPLAY_SONGS 200
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
    char count[32];               // "3 SONGS" ou "HIDDEN  3 SONGS"
} WeekRowText;

static WeekEntry* weeks;
static WeekRowText* rowTexts;
static int weekCount;
static int selected;
static float listOffset;

/* Tecla 7: mostra as weeks com hideStoryMode. Volta para escondido ao entrar na cena. */
static bool showHidden;
static char anchorFile[64];  // week a manter selecionada ao recarregar a lista
static char anchorDir[256];

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
static int detailsScroll; // primeira linha visivel da lista de faixas
static char trackDragging; // arrastando a barra de rolagem com o mouse
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
        if (weeks[i].hidden)
            snprintf(rowTexts[i].count, sizeof(rowTexts[i].count), "HIDDEN  %d SONGS", weeks[i].songCount);
        else
            snprintf(rowTexts[i].count, sizeof(rowTexts[i].count), "%d SONGS", weeks[i].songCount);
    }
}

/* Refaz o painel de detalhes da week selecionada. */
static void Details_Build(void) {
    if (selected < 0 || selected >= weekCount) return;

    const WeekEntry* week = weeks + selected;
    const int shown = week->songCount > MAX_DISPLAY_SONGS ? MAX_DISPLAY_SONGS
                    : week->songCount < 0 ? 0 : week->songCount;

    // Titulo alinhado a direita, no canto inferior.
    const Vector2 titleSize = MeasureTextEx(mainFont, WeekTitle(week), 36.0f, 2.0f);
    detailsTitleX = TITLE_RIGHT - titleSize.x;

    detailsHidden = week->songCount > MAX_DISPLAY_SONGS;
    if (detailsHidden)
        snprintf(detailsCount, sizeof(detailsCount), "SHOWING %d OF %d TRACKS", shown, week->songCount);
    else
        snprintf(detailsCount, sizeof(detailsCount), "%d TRACKS", week->songCount);

    detailsLineCount = shown;
    detailsScroll = 0;

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

    weekCount = WeekList_Scan(&weeks, showHidden);
    if (weekCount < 0) weekCount = 0;

    StoryCover_Reset(weekCount);

    if (anchorFile[0]) {
        for (int i = 0; i < weekCount; i++) {
            if (strcmp(weeks[i].file, anchorFile) == 0 && strcmp(weeks[i].dir, anchorDir) == 0) {
                selected = i;
                break;
            }
        }
    }

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

/* Tecla 7: alterna as weeks escondidas, mantendo a week atual selecionada. */
static void ToggleHiddenWeeks(void) {
    anchorFile[0] = '\0';
    anchorDir[0] = '\0';
    if (weeks && selected >= 0 && selected < weekCount) {
        snprintf(anchorFile, sizeof(anchorFile), "%s", weeks[selected].file);
        snprintf(anchorDir, sizeof(anchorDir), "%s", weeks[selected].dir);
    }

    showHidden = !showHidden;
    RefreshWeekList();

    anchorFile[0] = '\0';
    anchorDir[0] = '\0';
    SetStatus(showHidden ? "HIDDEN WEEKS: ON" : "HIDDEN WEEKS: OFF");
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
    backgroundTexture = MenuBackground_Get();

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
    showHidden = false;
    anchorFile[0] = '\0';
    anchorDir[0] = '\0';

    RefreshWeekList();

    if (WeekRun_Missing()[0]) {
        char message[128];
        snprintf(message, sizeof(message), "SKIPPED (MISSING): %.96s", WeekRun_Missing());
        SetStatus(message);
    }
}

static void ScrollTracks(int rows);
static void UpdateTrackScrollInput(void);

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

    if (IsKeyPressed(KEY_SEVEN)) {
        ToggleHiddenWeeks();
        return;
    }

    if (weekCount <= 0)
        return;

    // Roda do mouse rola a lista de faixas.
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
        ScrollTracks(wheel > 0.0f ? -1 : 1);

    UpdateTrackScrollInput();

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

/* Maior retangulo com a proporcao de (srcW x srcH) que cabe inteiro em `box`, centralizado. */
static Rectangle FitContain(float srcW, float srcH, Rectangle box) {
    const float scale = fminf(box.width / srcW, box.height / srcH);
    const float w = srcW * scale;
    const float h = srcH * scale;
    return (Rectangle) {box.x + (box.width - w) * 0.5f, box.y + (box.height - h) * 0.5f, w, h};
}

/* Banner da week inteiro (sem recorte), no topo ocupando a largura da tela. */
static void DrawCover(void) {
    const Rectangle slot = {BANNER_X, BANNER_Y, BANNER_W, BANNER_H};

    if (StoryCover_IsLoaded() && StoryCover_Texture().width > 0 && StoryCover_Texture().height > 0) {
        const Texture2D texture = StoryCover_Texture();
        const Rectangle art = FitContain((float)texture.width, (float)texture.height, slot);

        DrawTexturePro(texture, (Rectangle) {0.0f, 0.0f, (float)texture.width, (float)texture.height},
                       art, VECTOR_ZERO, 0.0f, WHITE);
        DrawRectangleLinesEx(art, 3.0f, (Color) {235, 220, 180, 230});
    } else {
        DrawRectangleRec(slot, (Color) {28, 33, 52, 255});
        DrawTextEx(mainFont, "NO WEEK ART", (Vector2) {BANNER_X + BANNER_W * 0.5f - 60.0f, BANNER_Y + 150.0f},
                   19.0f, 1.0f, COLOR_MUTED);
    }
}

/* Vinil pequeno no canto do banner. */
static void DrawDisc(void) {
    const Vector2 center = {DISC_CENTER_X, DISC_CENTER_Y};
    const float radius = DISC_RADIUS;
    const Vector2 marker = {center.x + 20.0f * cosf(discAngle), center.y + 20.0f * sinf(discAngle)};

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
    // So desenha linhas inteiras dentro da area da lista (nao invade o banner).
    if (y < LIST_Y || y + ROW_BOX_H > LIST_BOTTOM)
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

static int TrackRowsTotal(void) {
    return (detailsLineCount + DETAIL_COLUMNS - 1) / DETAIL_COLUMNS;
}

static int TrackRowsVisible(void) {
    const int visible = (int)(DETAIL_LIST_H / DETAIL_ROW_H);
    return visible < 1 ? 1 : visible;
}

static int TrackScrollMax(void) {
    const int max = TrackRowsTotal() - TrackRowsVisible();
    return max > 0 ? max : 0;
}

static void ScrollTracks(int rows) {
    detailsScroll += rows;
    if (detailsScroll < 0) detailsScroll = 0;
    if (detailsScroll > TrackScrollMax()) detailsScroll = TrackScrollMax();
}

static float TrackThumbH(void) {
    return fmaxf(28.0f, DETAIL_LIST_H * (float)TrackRowsVisible() / (float)TrackRowsTotal());
}

/* Area clicavel da barra (mais larga que a barra desenhada, para pegar com o mouse). */
static Rectangle TrackBarHit(void) {
    return (Rectangle) {DETAILS_X + DETAILS_W - 20.0f, DETAILS_Y + DETAIL_LIST_TOP, 22.0f, DETAIL_LIST_H};
}

/*
 * Rolagem das faixas: mouse arrastando a barra, roda do mouse e teclas O (sobe) / L (desce).
 * O e L nao sao usadas em outro lugar desta aba.
 */
static void UpdateTrackScrollInput(void) {
    const int maxScroll = TrackScrollMax();
    if (maxScroll <= 0) {
        trackDragging = 0;
        return;
    }

    const Vector2 mouse = RayGame_MouseWorld();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, TrackBarHit()))
        trackDragging = 1;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        trackDragging = 0;

    if (trackDragging) {
        const float thumbH = TrackThumbH();
        const float span = DETAIL_LIST_H - thumbH;
        if (span > 0.0f) {
            float t = (mouse.y - (DETAILS_Y + DETAIL_LIST_TOP) - thumbH * 0.5f) / span;
            t = fminf(fmaxf(t, 0.0f), 1.0f);
            detailsScroll = (int)lroundf(t * (float)maxScroll);
        }
    }

    if (IsKeyPressed(KEY_O)) ScrollTracks(-1);
    if (IsKeyPressed(KEY_L)) ScrollTracks(1);
}

/* Lista de faixas em colunas, com barra de rolagem quando nao cabe tudo. */
static void DrawTrackList(void) {
    const float listX = DETAILS_X + 18.0f;
    const float listY = DETAILS_Y + DETAIL_LIST_TOP;
    const int visible = TrackRowsVisible();
    const int total = TrackRowsTotal();

    for (int i = 0; i < detailsLineCount; ++i) {
        const int row = i / DETAIL_COLUMNS - detailsScroll;
        const int column = i % DETAIL_COLUMNS;
        if (row < 0 || row >= visible) continue;

        DrawTextEx(mainFont, detailsLines[i],
                   (Vector2) {listX + (float)column * DETAIL_COLUMN_W, listY + (float)row * DETAIL_ROW_H},
                   DETAIL_FONT, 1.0f, COLOR_TEXT);
    }

    if (total > visible) {
        const float barX = DETAILS_X + DETAILS_W - 14.0f;
        const float thumbH = TrackThumbH();
        const float thumbY = listY + (DETAIL_LIST_H - thumbH) * (float)detailsScroll / (float)TrackScrollMax();

        DrawRectangleRounded((Rectangle) {barX, listY, 6.0f, DETAIL_LIST_H}, 0.5f, 4, (Color) {40, 48, 70, 200});
        DrawRectangleRounded((Rectangle) {barX, thumbY, 6.0f, thumbH}, 0.5f, 4, COLOR_ACCENT);
    }
}

static void DrawWeekDetails(void) {
    const float firstX = DETAILS_X + 18.0f;

    const char* title = WeekTitle(weeks + selected);
    DrawTextEx(mainFont, title, (Vector2) {detailsTitleX + 2.0f, TITLE_Y + 2.0f}, 36.0f, 2.0f, (Color) {0, 0, 0, 180});
    DrawTextEx(mainFont, title, (Vector2) {detailsTitleX, TITLE_Y}, 36.0f, 2.0f, COLOR_TEXT);

    DrawPanel((Rectangle) {DETAILS_X, DETAILS_Y, DETAILS_W, DETAILS_H}, COLOR_PANEL, COLOR_BORDER);
    DrawTextEx(mainFont, detailsCount, (Vector2) {firstX, DETAILS_Y + 10.0f}, 14.0f, 1.0f, COLOR_ACCENT);

    DrawTrackList();

    if (detailsHidden)
        DrawTextEx(mainFont, "Additional tracks hidden (limit: 200)",
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

/* Fundo: a capa da week (ou o padrao) preenchendo a tela, recortado no centro sem esticar. */
static void DrawBackdrop(void) {
    const Texture2D background = StoryCover_IsLoaded() ? StoryCover_Texture() : backgroundTexture;
    if (background.id == 0) return;

    Rectangle source = {0.0f, 0.0f, (float)background.width, (float)background.height};
    const float screenAspect = (float)SCREEN_W / (float)SCREEN_H; // float: SCREEN_W / SCREEN_H daria 1
    const float textureAspect = source.width / source.height;
    if (textureAspect > screenAspect) {
        source.width = source.height * screenAspect;
        source.x = ((float)background.width - source.width) * 0.5f;
    } else {
        source.height = source.width / screenAspect;
        source.y = ((float)background.height - source.height) * 0.5f;
    }
    DrawTexturePro(background, source, (Rectangle) {0.0f, 0.0f, SCREEN_W, SCREEN_H},
                   VECTOR_ZERO, 0.0f, (Color) {115, 125, 160, 90});
}

static void DrawEmptyState(void) {
    DrawPanel((Rectangle) {250.0f, 245.0f, 780.0f, 190.0f}, COLOR_PANEL, COLOR_BORDER);
    DrawTextEx(mainFont, "NO WEEKS FOUND", (Vector2) {470.0f, 285.0f}, 34.0f, 2.0f, COLOR_TEXT);
    DrawTextEx(mainFont, "Add week JSON files to assets/weeks or assets/mods/<mod>/weeks.",
               (Vector2) {315.0f, 350.0f}, 18.0f, 1.0f, COLOR_MUTED);
}

static void DrawStatus(void) {
    if (statusTimer <= 0.0f || !statusText[0]) return;

    DrawPanel((Rectangle) {58.0f, 646.0f, statusWidth, 36.0f}, (Color) {23, 30, 43, 235}, COLOR_BORDER);
    DrawTextEx(mainFont, statusText, (Vector2) {75.0f, 654.0f}, 18.0f, 1.0f, COLOR_ACCENT);
}

static void DrawHelpLine(void) {
    DrawTextEx(mainFont, "UP / DOWN  SELECT     ENTER  PLAY     7  HIDDEN     DEL  DELETE     ESC  BACK",
               (Vector2) {58.0f, 690.0f}, 14.0f, 1.0f, COLOR_MUTED);
}

static void StoryMode_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&storyCamera);

    ClearBackground((Color) {7, 9, 18, 255});
    DrawBackdrop();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {5, 7, 15, 145});

    if (weekCount <= 0) {
        DrawEmptyState();
    } else {
        DrawWeekList();
        DrawCover();
        DrawWeekDetails();
        if (deleteConfirm)
            DrawDeleteModal();
    }

    DrawStatus();
    DrawHelpLine();

    Render_StopCamera();
}

static void StoryMode_Destroy([[maybe_unused]] RayScene* scene) {
    StoryCover_Free();

    backgroundTexture = (Texture2D) {0}; // compartilhada: nao descarrega aqui

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
