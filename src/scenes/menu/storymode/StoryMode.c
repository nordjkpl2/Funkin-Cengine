/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * StoryMode.c - selecao de weeks do Story Mode.
 *
 * A capa da week ocupa a tela inteira (recorte centralizado, sem esticar), com
 * gradientes em cima e embaixo para o texto ler bem. A busca da capa fica em
 * StoryCover.c. Textos e recortes que dependem so da week ou da capa sao montados
 * quando a lista ou a selecao muda, nunca no Draw.
 */

#include "scenes/LoadingState.h"
#include "scenes/api/EndStartLoader.h"
#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayState.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/storymode/StoryMode.h"
#include "data/song/SongList.h"
#include "scenes/menu/storymode/StoryCover.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"
#include "data/week/Week.h"
#include "rendering/TextMeasure.h"

#define SCREEN_W 1280
#define SCREEN_H 720

/* Layout: titulo grande no topo a esquerda; lista de weeks embaixo a esquerda; faixas a direita. */
#define HERO_TITLE_X 58.0f
#define HERO_TITLE_Y 36.0f
#define HERO_TITLE_SIZE 56.0f
#define HERO_TITLE_MAX_W 1000.0f
#define HERO_COUNT_Y 108.0f
#define DISC_CENTER_X 1222.0f
#define DISC_CENTER_Y 78.0f
#define DISC_RADIUS 22.0f

#define LIST_X 58.0f
#define LIST_Y 392.0f
#define LIST_W 560.0f
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
static const Color COLOR_BORDER = {90, 105, 145, 150};
static const Color COLOR_TEXT = {238, 241, 255, 255};
static const Color COLOR_MUTED = {155, 165, 190, 255};
static const Color COLOR_ACCENT = {255, 205, 92, 255};
static const Color COLOR_GREEN = {100, 225, 155, 255};
static const Color COLOR_DANGER = {255, 105, 115, 255};
static const Color COLOR_SHADE = {5, 7, 15, 255};

/* Textos prontos de cada week na lista. */
typedef struct {
    char idleTitle[TEXT_MAX];     // cortado para caber, fonte normal
    char selectedTitle[TEXT_MAX]; // cortado para caber, fonte da selecionada
    char count[32];               // "3 SONGS" ou "HIDDEN  3 SONGS"
    float countWidth;             // largura da contagem, para desenhar a pilula
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

/* Fundo: capa (ou padrao) ja recortada para preencher a tela. Recalculado so ao trocar de capa. */
static Texture2D backdrop;
static Rectangle backdropSrc;
static Rectangle coverFitDst; // capa inteira (contain) por cima do fundo; width 0 = nao desenhar
static Texture2D backgroundTexture;
static Camera2D storyCamera;
static float discAngle;
static int diffIndex = 1;   // 0 easy, 1 normal, 2 hard (padrao normal, como Psych/original)
static const char* const DIFF_IMAGES[3] = {"assets/images/menudifficulties/easy.png",
                                           "assets/images/menudifficulties/normal.png",
                                           "assets/images/menudifficulties/hard.png"};
static Texture2D diffTex[3];
#define DIFF_IMG_H 24.0f   // altura da imagem da dificuldade na pilula
static const Color DIFF_COLORS[3] = {{100, 225, 155, 255}, {255, 205, 92, 255}, {255, 105, 115, 255}};

/* Animacoes curtas (0 -> 1): entrada do titulo ao trocar de week e troca de dificuldade. */
#define HERO_ANIM_SPEED 4.0f
#define DIFF_ANIM_SPEED 8.0f
static float heroAnim = 1.0f;
static float diffAnim = 1.0f;

static char listCounter[32]; // "WEEK 2 / 7"

/* Linha de atalhos: posicoes medidas uma vez no Create. */
#define HELP_COUNT 6
#define HELP_Y 692.0f
#define HELP_FONT 13.0f
static const char* const HELP_KEYS[HELP_COUNT] = {"UP / DOWN", "LEFT / RIGHT", "ENTER", "7", "DEL", "ESC"};
static const char* const HELP_LABELS[HELP_COUNT] = {"SELECT", "DIFFICULTY", "PLAY", "HIDDEN", "DELETE", "BACK"};
static float helpKeyX[HELP_COUNT], helpKeyW[HELP_COUNT], helpLabelX[HELP_COUNT];

static char deleteConfirm;
static char deletePrompt[TEXT_MAX];
static float deletePromptX; // centralizado no modal

static char statusText[128];
static float statusWidth;
static float statusTimer;

/* Titulo grande e contagem da week selecionada, e painel de faixas. */
static char heroTitle[TEXT_MAX];
static char heroCount[64];
static char detailsCount[64];
static char detailsLastScore[48];
static char detailsLines[MAX_DISPLAY_SONGS][DETAIL_LINE_MAX]; // so o nome
static char detailsNums[MAX_DISPLAY_SONGS][4];               // "01".."200"
static float detailsNameDx; // distancia do numero ao nome
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

/* Copia o titulo e corta com "..." ate caber em maxWidth. */
static void FitTitle(const char* title, float fontSize, float maxWidth, char out[TEXT_MAX]) {
    size_t length = strlen(title);
    if (length >= TEXT_MAX) length = TEXT_MAX - 1;
    memcpy(out, title, length);
    out[length] = '\0';

    if (MeasureTextEx(mainFont, out, fontSize, 1.5f).x <= maxWidth)
        return;

    while (length > 4 && MeasureTextEx(mainFont, out, fontSize, 1.5f).x > maxWidth)
        out[--length] = '\0';

    if (length > 3)
        memcpy(out + length - 3, "...", 3);
}

static void RowTexts_Build(void) {
    for (int i = 0; i < weekCount; i++) {
        const char* title = WeekTitle(weeks + i);
        FitTitle(title, ROW_FONT_IDLE, TITLE_MAX_W, rowTexts[i].idleTitle);
        FitTitle(title, ROW_FONT_SELECTED, TITLE_MAX_W, rowTexts[i].selectedTitle);
        if (weeks[i].hidden)
            snprintf(rowTexts[i].count, sizeof(rowTexts[i].count), "HIDDEN  %d SONGS", weeks[i].songCount);
        else
            snprintf(rowTexts[i].count, sizeof(rowTexts[i].count), "%d SONGS", weeks[i].songCount);
        rowTexts[i].countWidth = MeasureTextEx(mainFont, rowTexts[i].count, 13.0f, 1.0f).x;
    }
}

/* Refaz o titulo grande e o painel de faixas da week selecionada. */
static void Details_Build(void) {
    if (selected < 0 || selected >= weekCount) return;

    const WeekEntry* week = weeks + selected;
    const int shown = week->songCount > MAX_DISPLAY_SONGS ? MAX_DISPLAY_SONGS
                    : week->songCount < 0 ? 0 : week->songCount;

    FitTitle(WeekTitle(week), HERO_TITLE_SIZE, HERO_TITLE_MAX_W, heroTitle);
    if (week->hidden)
        snprintf(heroCount, sizeof(heroCount), "HIDDEN   %d SONGS", week->songCount);
    else
        snprintf(heroCount, sizeof(heroCount), "%d SONGS", week->songCount);

    detailsHidden = week->songCount > MAX_DISPLAY_SONGS;
    if (detailsHidden)
        snprintf(detailsCount, sizeof(detailsCount), "SHOWING %d OF %d TRACKS", shown, week->songCount);
    else
        snprintf(detailsCount, sizeof(detailsCount), "%d TRACKS", week->songCount);

    detailsLineCount = shown;
    detailsScroll = 0;

    for (int i = 0; i < shown; i++) {
        const char* name = week->songs[i].song[0] ? week->songs[i].song : "Unknown";
        snprintf(detailsNums[i], sizeof(detailsNums[i]), "%02d", i + 1);
        snprintf(detailsLines[i], DETAIL_LINE_MAX, "%.17s", name);
    }
    detailsNameDx = MeasureTextEx(mainFont, shown >= 100 ? "000" : "00", DETAIL_FONT, 1.0f).x + 12.0f;

    const int lastScore = WeekRun_LastTotal(week);
    if (lastScore > 0)
        snprintf(detailsLastScore, sizeof(detailsLastScore), "LAST SCORE  %d", lastScore);
    else
        detailsLastScore[0] = '\0';
}

/*
 * Recorte que preenche a tela (cover), centralizado, sem esticar. Usa a capa se
 * houver, senao o fundo do menu. Chamado so ao trocar de week.
 */
static void Backdrop_Update(void) {
    backdrop = StoryCover_IsLoaded() ? StoryCover_Texture() : backgroundTexture;
    coverFitDst = (Rectangle) {0};
    if (backdrop.id == 0 || backdrop.width <= 0 || backdrop.height <= 0) return;

    const float screenAspect = (float)SCREEN_W / (float)SCREEN_H; // float: SCREEN_W / SCREEN_H daria 1
    const float textureAspect = (float)backdrop.width / (float)backdrop.height;

    // Capa inteira, proporcional, centralizada (contain), com margem.
    if (StoryCover_IsLoaded()) {
        const float margin = 40.0f;
        const float maxW = SCREEN_W - margin * 2.0f;
        const float maxH = SCREEN_H - margin * 2.0f;
        float w = maxW;
        float h = w / textureAspect;
        if (h > maxH) {
            h = maxH;
            w = h * textureAspect;
        }
        coverFitDst = (Rectangle) {(SCREEN_W - w) * 0.5f, (SCREEN_H - h) * 0.5f, w, h};
    }

    // Fundo: recorte que preenche a tela (como antes).
    backdropSrc = (Rectangle) {0.0f, 0.0f, (float)backdrop.width, (float)backdrop.height};
    if (textureAspect > screenAspect) {
        backdropSrc.width = backdropSrc.height * screenAspect;
        backdropSrc.x = ((float)backdrop.width - backdropSrc.width) * 0.5f;
    } else {
        backdropSrc.height = backdropSrc.width / screenAspect;
        backdropSrc.y = ((float)backdrop.height - backdropSrc.height) * 0.5f;
    }
}

/* ------------------------------------------------------------------ */
/* Lista de weeks e selecao                                            */
/* ------------------------------------------------------------------ */

static void SelectWeek(int index) {
    selected = index;
    StoryCover_Load(selected, weeks + selected);
    Backdrop_Update();
    Details_Build();
    snprintf(listCounter, sizeof(listCounter), "WEEK %d / %d", selected + 1, weekCount);
    heroAnim = 0.0f;
}

/* Posicoes da linha de atalhos: tecla (dentro de uma "tecla" desenhada) e rotulo. */
static void HelpLayout_Build(void) {
    float x = 58.0f;
    for (int i = 0; i < HELP_COUNT; i++) {
        helpKeyX[i] = x + 6.0f;
        helpKeyW[i] = MeasureTextEx(mainFont, HELP_KEYS[i], HELP_FONT, 1.0f).x;
        helpLabelX[i] = helpKeyX[i] + helpKeyW[i] + 14.0f;
        x = helpLabelX[i] + MeasureTextEx(mainFont, HELP_LABELS[i], HELP_FONT, 1.0f).x + 26.0f;
    }
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

    WeekRun_Start(weeks + selected, diffIndex);
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
    diffAnim = 1.0f;
    for (int i = 0; i < 3; i++) diffTex[i] = Render_LoadTexture(DIFF_IMAGES[i]);

    HelpLayout_Build();
    RefreshWeekList();
    Backdrop_Update();

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
    heroAnim = fminf(heroAnim + dt * HERO_ANIM_SPEED, 1.0f);
    diffAnim = fminf(diffAnim + dt * DIFF_ANIM_SPEED, 1.0f);

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

    // Dificuldade: setas esquerda/direita (A/D), com volta, como as setas da Psych.
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        diffIndex = (diffIndex + 2) % 3;
        diffAnim = 0.0f;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        diffIndex = (diffIndex + 1) % 3;
        diffAnim = 0.0f;
    }

    UpdateTrackScrollInput();

    // Roda do mouse move a selecao do menu (cima/baixo), como as setas.
    const float wheel = GetMouseWheelMove();
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || wheel < 0.0f)
        SelectWeek((selected + 1) % weekCount);
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || wheel > 0.0f)
        SelectWeek((selected - 1 + weekCount) % weekCount);

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_SLASH) || IsKeyPressed(KEY_KP_DIVIDE)) {
        char prompt[TEXT_MAX];
        snprintf(prompt, sizeof(prompt), "DELETE %.80s?", WeekTitle(weeks + selected));
        FitTitle(prompt, 30.0f, 600.0f, deletePrompt); // cabe no modal
        deletePromptX = (SCREEN_W - MeasureTextEx(mainFont, deletePrompt, 30.0f, 1.5f).x) * 0.5f;
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
    DrawRectangleRoundedLinesEx(rect, 0.045f, 8, 1.0f, border); // borda segue os cantos arredondados
}

/* Curva de saida cubica: rapida no inicio, assenta no fim. */
static float EaseOut(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

/* Pulso 0..1 para brilhos. Multiplo inteiro de discAngle: continuo quando ele volta a 0. */
static float Pulse(void) {
    return 0.5f + 0.5f * sinf(discAngle * 8.0f);
}

/* Fundo em tela cheia (recorte), capa inteira por cima, e gradientes para leitura. */
static void DrawCover(void) {
    if (backdrop.id != 0)
        DrawTexturePro(backdrop, backdropSrc, (Rectangle) {0.0f, 0.0f, SCREEN_W, SCREEN_H},
                       VECTOR_ZERO, 0.0f, WHITE);

    if (coverFitDst.width > 0.0f) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {5, 7, 15, 150}); // escurece o fundo
        DrawTexturePro(backdrop, (Rectangle) {0.0f, 0.0f, (float)backdrop.width, (float)backdrop.height},
                       coverFitDst, VECTOR_ZERO, 0.0f, WHITE);
    }

    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {5, 7, 15, 40});
    DrawRectangleGradientV(0, 0, SCREEN_W, 250, (Color) {5, 7, 15, 215}, (Color) {5, 7, 15, 0});
    DrawRectangleGradientV(0, 290, SCREEN_W, SCREEN_H - 290, (Color) {5, 7, 15, 0}, (Color) {5, 7, 15, 240});
}

/* Vinil pequeno no canto superior direito. */
static void DrawDisc(void) {
    const Vector2 center = {DISC_CENTER_X, DISC_CENTER_Y};
    const float radius = DISC_RADIUS;
    const Vector2 marker = {center.x + 16.0f * cosf(discAngle), center.y + 16.0f * sinf(discAngle)};

    DrawCircleV(center, radius + 4.0f, (Color) {4, 6, 13, 150});
    DrawCircleV(center, radius, (Color) {19, 22, 35, 255});
    DrawCircleLines((int)center.x, (int)center.y, radius - 2.0f, (Color) {105, 117, 150, 210});
    DrawCircleLines((int)center.x, (int)center.y, 14.0f, (Color) {62, 72, 103, 190});
    DrawCircleV(center, 6.0f, (Color) {104, 79, 153, 255});
    DrawCircleV(center, 2.0f, (Color) {231, 232, 242, 255});
    DrawLineEx(center, marker, 1.5f, (Color) {255, 218, 139, 210});
    DrawCircleV(marker, 2.0f, COLOR_ACCENT);
}

/*
 * Titulo grande da week selecionada (entra deslizando ao trocar de week), contagem e
 * seletor de dificuldade "< NORMAL >" na cor da dificuldade. Formas primeiro, depois textos.
 */
static void DrawHero(void) {
    static TextMeasure countMeasure;   // texto muda so ao trocar de week
    const Color diffColor = DIFF_COLORS[diffIndex];
    const Texture2D img = diffTex[diffIndex];
    const float imgW = img.height > 0 ? DIFF_IMG_H * (float)img.width / (float)img.height : 0.0f;
    const float countW = TextMeasure_Get(&countMeasure, mainFont, heroCount, 16.0f, 1.5f).x;
    const float diffX = HERO_TITLE_X + 2.0f + countW + 26.0f;
    const Rectangle pill = {diffX - 10.0f, HERO_COUNT_Y - 9.0f, imgW + 62.0f, DIFF_IMG_H + 10.0f};
    const float enter = EaseOut(heroAnim);
    const float titleX = HERO_TITLE_X - (1.0f - enter) * 28.0f;
    const float diffY = pill.y + 5.0f - (1.0f - EaseOut(diffAnim)) * 6.0f;

    DrawRectangleRounded(pill, 0.5f, 8, Fade(diffColor, 0.14f));
    DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.0f, Fade(diffColor, 0.6f));
    // Traco de acento sob a contagem, cresce junto com a entrada do titulo.
    DrawRectangleRounded((Rectangle) {HERO_TITLE_X + 2.0f, HERO_COUNT_Y + 34.0f, 28.0f + 52.0f * enter, 3.0f},
                         1.0f, 4, COLOR_ACCENT);

    DrawTextEx(mainFont, "STORY MODE", (Vector2) {HERO_TITLE_X + 2.0f, HERO_TITLE_Y - 18.0f},
               13.0f, 4.0f, Fade(COLOR_ACCENT, 0.8f));
    DrawTextEx(mainFont, heroTitle, (Vector2) {titleX + 3.0f, HERO_TITLE_Y + 3.0f},
               HERO_TITLE_SIZE, 1.5f, Fade(BLACK, 0.75f * enter));
    DrawTextEx(mainFont, heroTitle, (Vector2) {titleX, HERO_TITLE_Y},
               HERO_TITLE_SIZE, 1.5f, Fade(COLOR_TEXT, enter));
    DrawTextEx(mainFont, heroCount, (Vector2) {HERO_TITLE_X + 2.0f, HERO_COUNT_Y},
               16.0f, 1.5f, COLOR_ACCENT);
    DrawTextEx(mainFont, "<", (Vector2) {diffX, HERO_COUNT_Y}, 16.0f, 1.5f, diffColor);
    DrawTexturePro(img, (Rectangle) {0.0f, 0.0f, (float)img.width, (float)img.height},
                   (Rectangle) {diffX + 21.0f, diffY, imgW, DIFF_IMG_H}, (Vector2) {0}, 0.0f,
                   Fade(WHITE, 0.4f + 0.6f * EaseOut(diffAnim)));
    DrawTextEx(mainFont, ">", (Vector2) {pill.x + pill.width - 18.0f, HERO_COUNT_Y}, 16.0f, 1.5f, diffColor);
}

/* Retangulo da linha i; false se ela nao cabe inteira na area da lista. */
static bool WeekRowRect(int i, Rectangle* row) {
    const float y = LIST_Y + ((float)i - listOffset) * ROW_H;
    // So desenha linhas inteiras dentro da area da lista.
    if (y < LIST_Y || y + ROW_BOX_H > LIST_BOTTOM)
        return false;
    *row = (Rectangle) {LIST_X, y, LIST_W, ROW_BOX_H};
    return true;
}

/* Passada de formas: sombra, fundo, borda, barra de acento e pilula da contagem. */
static void DrawWeekRowShapes(int i) {
    Rectangle row;
    if (!WeekRowRect(i, &row))
        return;
    const bool isSelected = i == selected;
    const WeekRowText* text = rowTexts + i;

    // Sombra suave embaixo da linha.
    DrawRectangleRounded((Rectangle) {row.x, row.y + 4.0f, row.width, row.height},
                         0.16f, 8, (Color) {3, 4, 10, 110});

    if (isSelected) {
        // Brilho dourado pulsando por fora, fundo com faixa clara no topo (simula gradiente) e borda.
        DrawRectangleRounded((Rectangle) {row.x - 4.0f, row.y - 4.0f, row.width + 8.0f, row.height + 8.0f},
                             0.22f, 8, Fade(COLOR_ACCENT, 0.08f + 0.1f * Pulse()));
        DrawRectangleRounded(row, 0.16f, 8, (Color) {44, 38, 62, 248});
        DrawRectangleRounded((Rectangle) {row.x + 1.0f, row.y + 1.0f, row.width - 2.0f, row.height * 0.5f},
                             0.3f, 8, (Color) {255, 255, 255, 16});
        DrawRectangleRoundedLinesEx(row, 0.16f, 8, 1.5f, (Color) {255, 205, 92, 220});
    } else {
        DrawRectangleRounded(row, 0.16f, 8, (Color) {14, 18, 33, 200});
        DrawRectangleRoundedLinesEx(row, 0.16f, 8, 1.0f, (Color) {70, 84, 120, 110});
    }

    // Contagem dentro de uma pilula.
    const float countX = row.x + LIST_W - 112.0f;
    const Rectangle pill = {countX - 10.0f, row.y + (ROW_BOX_H - 24.0f) * 0.5f, text->countWidth + 20.0f, 24.0f};
    DrawRectangleRounded(pill, 0.5f, 8, isSelected ? (Color) {255, 205, 92, 44} : (Color) {255, 255, 255, 12});
}

/* Passada de textos: seta, titulo (com sombra quando selecionado) e contagem. */
static void DrawWeekRowText(int i) {
    Rectangle row;
    if (!WeekRowRect(i, &row))
        return;
    const bool isSelected = i == selected;
    const WeekRowText* text = rowTexts + i;

    // Seta ">" no lugar do seletor, como no freeplay, balancando de leve.
    if (isSelected)
        DrawTextEx(mainFont, ">", (Vector2) {row.x + 3.0f + 3.0f * Pulse(), row.y + 14.0f}, 34.0f, 1.0f, COLOR_ACCENT);

    // Titulo, com sombra leve quando selecionado.
    const Vector2 titlePos = {row.x + 25.0f, row.y + 13.0f};
    if (isSelected)
        DrawTextEx(mainFont, text->selectedTitle, (Vector2) {titlePos.x + 1.0f, titlePos.y + 2.0f},
                   ROW_FONT_SELECTED, 1.5f, (Color) {0, 0, 0, 160});
    DrawTextEx(mainFont, isSelected ? text->selectedTitle : text->idleTitle, titlePos,
               isSelected ? ROW_FONT_SELECTED : ROW_FONT_IDLE, 1.5f,
               isSelected ? COLOR_TEXT : COLOR_MUTED);

    const float countX = row.x + LIST_W - 112.0f;
    DrawTextEx(mainFont, text->count, (Vector2) {countX, row.y + (ROW_BOX_H - 24.0f) * 0.5f + 6.0f},
               13.0f, 1.0f, isSelected ? COLOR_ACCENT : COLOR_MUTED);
}

/* Lista de semanas em duas passadas (formas, depois textos) para cortar trocas de textura. */
static void DrawWeekList(void) {
    int first = (int)floorf(listOffset - VISIBLE_MARGIN / ROW_H) - 1;
    int last = (int)ceilf(listOffset + (LIST_BOTTOM - LIST_Y + VISIBLE_MARGIN) / ROW_H) + 1;
    if (first < 0) first = 0;
    if (last >= weekCount) last = weekCount - 1;

    for (int i = first; i <= last; ++i)
        DrawWeekRowShapes(i);
    for (int i = first; i <= last; ++i)
        DrawWeekRowText(i);

    // Posicao na lista, acima dela.
    DrawTextEx(mainFont, listCounter, (Vector2) {LIST_X + 2.0f, LIST_Y - 22.0f}, 13.0f, 3.0f, COLOR_MUTED);
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
/* Barra de rolagem da lista de faixas (formas). */
static void DrawTrackScrollbar(void) {
    const float listY = DETAILS_Y + DETAIL_LIST_TOP;
    const int visible = TrackRowsVisible();
    const int total = TrackRowsTotal();

    if (total > visible) {
        const float barX = DETAILS_X + DETAILS_W - 14.0f;
        const float thumbH = TrackThumbH();
        const float thumbY = listY + (DETAIL_LIST_H - thumbH) * (float)detailsScroll / (float)TrackScrollMax();

        DrawRectangleRounded((Rectangle) {barX, listY, 6.0f, DETAIL_LIST_H}, 0.5f, 4, (Color) {40, 48, 70, 200});
        DrawRectangleRounded((Rectangle) {barX, thumbY, 6.0f, thumbH}, 0.5f, 4, COLOR_ACCENT);
    }
}

/* Nomes das faixas (textos). */
static void DrawTrackTexts(void) {
    const float listX = DETAILS_X + 18.0f;
    const float listY = DETAILS_Y + DETAIL_LIST_TOP;
    const int visible = TrackRowsVisible();

    for (int i = 0; i < detailsLineCount; ++i) {
        const int row = i / DETAIL_COLUMNS - detailsScroll;
        const int column = i % DETAIL_COLUMNS;
        if (row < 0 || row >= visible) continue;

        const Vector2 pos = {listX + (float)column * DETAIL_COLUMN_W, listY + (float)row * DETAIL_ROW_H};
        DrawTextEx(mainFont, detailsNums[i], pos, DETAIL_FONT, 1.0f, Fade(COLOR_ACCENT, 0.7f));
        DrawTextEx(mainFont, detailsLines[i], (Vector2) {pos.x + detailsNameDx, pos.y},
                   DETAIL_FONT, 1.0f, COLOR_TEXT);
    }
}

/* Painel de detalhes: painel e barra (formas) antes de todos os textos. */
static void DrawWeekDetails(void) {
    const float firstX = DETAILS_X + 18.0f;

    DrawPanel((Rectangle) {DETAILS_X, DETAILS_Y, DETAILS_W, DETAILS_H}, COLOR_PANEL, COLOR_BORDER);
    // Linha separando o cabecalho da lista.
    DrawRectangle((int)firstX, (int)(DETAILS_Y + 30.0f), (int)(DETAILS_W - 36.0f), 1, Fade(COLOR_BORDER, 0.5f));
    DrawTrackScrollbar();

    DrawTextEx(mainFont, detailsCount, (Vector2) {firstX, DETAILS_Y + 10.0f}, 14.0f, 1.0f, COLOR_ACCENT);
    DrawTrackTexts();

    if (detailsHidden)
        DrawTextEx(mainFont, "Additional tracks hidden (limit: 200)",
                   (Vector2) {firstX, DETAILS_Y + DETAILS_H - 17.0f}, 10.0f, 1.0f, COLOR_MUTED);

    if (detailsLastScore[0])
        DrawTextEx(mainFont, detailsLastScore, (Vector2) {DETAILS_X + DETAILS_W - 170.0f, DETAILS_Y + 9.0f},
                   12.0f, 1.0f, COLOR_GREEN);
}

static void DrawDeleteModal(void) {
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color) {0, 0, 0, 165});
    DrawPanel((Rectangle) {300.0f, 260.0f, 680.0f, 175.0f}, (Color) {27, 20, 34, 250}, COLOR_DANGER);
    DrawRectangle(340, 345, 600, 1, Fade(COLOR_DANGER, 0.35f));
    DrawTextEx(mainFont, deletePrompt, (Vector2) {deletePromptX, 294.0f}, 30.0f, 1.5f, COLOR_TEXT);
    DrawTextEx(mainFont, "Y  CONFIRM", (Vector2) {420.0f, 370.0f}, 19.0f, 1.0f, COLOR_DANGER);
    DrawTextEx(mainFont, "N  CANCEL", (Vector2) {720.0f, 370.0f}, 19.0f, 1.0f, COLOR_GREEN);
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

/* Atalhos: cada tecla numa "tecla" arredondada, rotulo ao lado. Formas antes dos textos. */
static void DrawHelpLine(void) {
    for (int i = 0; i < HELP_COUNT; i++) {
        const Rectangle cap = {helpKeyX[i] - 6.0f, HELP_Y - 3.0f, helpKeyW[i] + 12.0f, HELP_FONT + 6.0f};
        DrawRectangleRounded(cap, 0.35f, 6, (Color) {255, 255, 255, 18});
        DrawRectangleRoundedLinesEx(cap, 0.35f, 6, 1.0f, (Color) {155, 165, 190, 90});
    }
    for (int i = 0; i < HELP_COUNT; i++) {
        DrawTextEx(mainFont, HELP_KEYS[i], (Vector2) {helpKeyX[i], HELP_Y}, HELP_FONT, 1.0f, COLOR_TEXT);
        DrawTextEx(mainFont, HELP_LABELS[i], (Vector2) {helpLabelX[i], HELP_Y}, HELP_FONT, 1.0f, COLOR_MUTED);
    }
}

static void StoryMode_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&storyCamera);

    ClearBackground(COLOR_SHADE);
    DrawCover();

    if (weekCount <= 0) {
        DrawEmptyState();
    } else {
        DrawHero();
        DrawWeekList();
        DrawWeekDetails();
        DrawDisc();
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
    backdrop = (Texture2D) {0};
    for (int i = 0; i < 3; i++) {
        UnloadTexture(diffTex[i]);
        diffTex[i] = (Texture2D) {0};
    }
    coverFitDst = (Rectangle) {0};

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
