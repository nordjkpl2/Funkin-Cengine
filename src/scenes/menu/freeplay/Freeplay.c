/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * Freeplay.c - seletor de musicas: lista a esquerda, card da musica selecionada a direita.
 * Visual inspirado no Freeplay do FNF e em seletores de jogos de ritmo: fundo com o icone
 * esmaecido, linha selecionada em destaque, card com capa e contador.
 * Icones: a busca e a decodificacao sao pedidas com antecedencia (vizinhos da selecao),
 * entao trocar de musica nao trava a thread principal.
 */

#include "scenes/api/FontLoader.h"
#include "scenes/gameplay/PlayState.h"
#include "scenes/LoadingState.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/freeplay/Freeplay.h"
#include "data/song/SongList.h"
#include "scenes/menu/freeplay/FreeplayIcon.h"
#include "ui/SearchBox.h"
#include "rendering/TextMeasure.h"
#include <stdlib.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Game.h"

#define SCREEN_W 1280.0f
#define SCREEN_H 720.0f

/* Lista */
#define ROW_X 128.0f            /* x da linha selecionada */
#define ROW_CENTER_Y 372.0f     /* y do centro da linha selecionada */
#define ROW_STEP 74.0f
#define ROW_VISIBLE 7.0f        /* linhas desenhadas para cada lado */
#define ROW_INDENT 24.0f        /* recuo por linha de distancia */
#define ROW_MAX_W 680.0f        /* largura maxima da pilula (antes do card) */
#define LIST_SCROLL_SPEED 10.0f
#define SELECT_EASE 12.0f       /* velocidade do pulso ao trocar de musica */
#define WARM_RADIUS 4           /* quantas musicas de cada lado ja ficam com icone pronto */

/* Card */
#define CARD_X 852.0f
#define CARD_Y 96.0f
#define CARD_W 392.0f
#define CARD_H 520.0f
#define COVER_SIZE 300.0f
#define COVER_X (CARD_X + (CARD_W - COVER_SIZE) * 0.5f)
#define COVER_Y (CARD_Y + 26.0f)
#define CARD_PAD 24.0f

static const Color ACCENT = {255, 92, 176, 255};          /* rosa FNF */
static const Color CYAN = {120, 230, 255, 255};
static const Color TEXT_IDLE = {200, 196, 226, 255};
static const Color TEXT_DIM = {150, 146, 178, 255};
static const Color ROW_BG = {40, 26, 72, 235};
static const Color CARD_BG = {12, 10, 26, 222};
static const Color CARD_EDGE = {120, 100, 200, 150};
static const Color SHADOW = {0, 0, 0, 120};
static const Color ICON_EMPTY_BG = {36, 32, 58, 255};
static const Color ICON_EMPTY_TEXT = {150, 145, 170, 255};

static SongEntry* songs;
static int songCount;
/* Lista filtrada: shown[linha] = indice em songs. selectedSong e uma linha de shown. */
static int* shown;
static int shownCount;
static int selectedSong;
static SearchBox search;
static float listOffset;
static float selectPulse = 1.0f;   /* 0 logo apos trocar a musica, sobe ate 1 */
static float backFade = 1.0f;      /* 0 logo apos trocar, sobe ate 1 (fundo com o icone) */

static RayGraphicObject bg;
static Camera2D cam;

/* Layout do titulo: mede so quando a selecao muda. */
static float titleSize;
static float titleX;
static char counterText[48];

/* ------------------------------------------------------------------ */
/* Selecao                                                             */
/* ------------------------------------------------------------------ */

static const SongEntry* CurrentSong(void) {
    return songs + shown[selectedSong];
}

/* Pede icones dos vizinhos em background: quando o jogador descer, o icone ja estara pronto. */
static void WarmAround(void) {
    for (int d = 1; d <= WARM_RADIUS; ++d) {
        if (selectedSong + d < shownCount) FreeplayIcon_Warm(songs + shown[selectedSong + d]);
        if (selectedSong - d >= 0) FreeplayIcon_Warm(songs + shown[selectedSong - d]);
    }
}

static void SelectSong(void) {
    FreeplayIcon_Load(CurrentSong());

    // Titulo cabe no card: diminui a fonte se for longo (so ao trocar de musica).
    titleSize = 34.0f;
    static TextMeasure titleMeasure;
    Vector2 size = TextMeasure_Get(&titleMeasure, mainFont, CurrentSong()->name, titleSize, 2.0f);
    if (size.x > CARD_W - 2.0f * CARD_PAD) {
        titleSize = fmaxf(18.0f, titleSize * (CARD_W - 2.0f * CARD_PAD) / size.x);
        size = MeasureTextEx(mainFont, CurrentSong()->name, titleSize, 2.0f);
    }
    titleX = CARD_X + (CARD_W - size.x) * 0.5f;

    snprintf(counterText, sizeof(counterText), "%02d / %02d", selectedSong + 1, shownCount);

    selectPulse = 0.0f;
    backFade = 0.0f;
    WarmAround();
}

/* Refaz a lista filtrada; mantem a musica atual se ainda aparecer. So roda ao aplicar a busca. */
static void ApplyFilter(void) {
    const int keep = shownCount > 0 ? shown[selectedSong] : -1;
    int row = 0;

    shownCount = 0;
    for (int i = 0; i < songCount; ++i) {
        if (!SearchBox_Match(&search, songs[i].name)) continue;
        if (i == keep) row = shownCount;
        shown[shownCount++] = i;
    }

    selectedSong = row;
    listOffset = (float)row;

    if (shownCount > 0) SelectSong();
    else FreeplayIcon_Load(NULL);
}

static void PlaySelected(void) {
    const char* dir = CurrentSong()->dir;

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
    free(shown);
    shown = NULL;
    shownCount = 0;
    SearchBox_Init(&search, (Rectangle) {105.0f, 14.0f, 420.0f, 44.0f});

    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = MenuBackground_Get();

    cam = (Camera2D) {
        .target = {SCREEN_W / 2, SCREEN_H / 2},
        .zoom = 1.0f
    };

    songCount = SongList_Scan(&songs);
    if (songCount < 0) songCount = 0;

    selectedSong = 0;
    listOffset = 0.0f;

    if (songCount > 0) {
        shown = malloc((size_t)songCount * sizeof(int));
        if (shown == NULL) {
            SongList_Free(songs);
            songs = NULL;
            songCount = 0;
            return;
        }
        for (int i = 0; i < songCount; ++i) shown[i] = i;
        shownCount = songCount;
        FreeplayIcon_Init(songs, songCount);
        SelectSong();
    }
}

static void Freeplay_Update([[maybe_unused]] RayScene* scene) {
    const float dt = fmaxf(RayGame_DeltaTime(), 0.0f);

    if (songCount <= 0) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) MenuState_SetScene();
        return;
    }

    // Letras, Backspace, Esc e Enter pertencem ao campo enquanto ele estava focado.
    const bool typing = SearchBox_Focused(&search);
    if (SearchBox_Update(&search)) ApplyFilter();

    if (!typing && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE))) {
        MenuState_SetScene();
        return;
    }

    if (shownCount > 0) {
        const int previous = selectedSong;
        const bool down = IsKeyPressed(KEY_DOWN) || (!typing && IsKeyPressed(KEY_S));
        const bool up = IsKeyPressed(KEY_UP) || (!typing && IsKeyPressed(KEY_W));

        if (down) {
            if (++selectedSong >= shownCount) selectedSong = 0;
        } else if (up) {
            if (--selectedSong < 0) selectedSong = shownCount - 1;
        }

        // Roda do mouse: so faz algo quando rolada. Shift = 5 itens. Sem dar a volta.
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            const int step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 5 : 1;
            selectedSong += wheel > 0.0f ? -step : step;
            if (selectedSong < 0) selectedSong = 0;
            if (selectedSong >= shownCount) selectedSong = shownCount - 1;
        }

        if (selectedSong != previous)
            SelectSong();

        if (!typing && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
            PlaySelected();
            return;
        }
    }

    // Rolagem e pulsos independentes de framerate.
    const float factor = 1.0f - expf(-LIST_SCROLL_SPEED * dt);
    listOffset += ((float)selectedSong - listOffset) * factor;

    const float ease = 1.0f - expf(-SELECT_EASE * dt);
    selectPulse += (1.0f - selectPulse) * ease;
    backFade += (1.0f - backFade) * ease;
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

/* Fundo: o icone da musica, grande e esmaecido, por tras de um gradiente escuro. */
static void DrawBackdrop(void) {
    Render_DrawGraphicObject(&bg);
    DrawRectangle(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){8, 6, 18, 150});

    if (FreeplayIcon_IsLoaded()) {
        const Texture2D texture = FreeplayIcon_Texture();
        const float a = 1.0f * backFade;
        DrawTexturePro(texture, SquareCrop(texture), (Rectangle){-120.0f, -40.0f, SCREEN_W + 240.0f, SCREEN_W + 240.0f},
                       (Vector2){0.0f, 0.0f}, 0.0f, Fade(WHITE, a));
    }

    DrawRectangleGradientV(0, 0, (int)SCREEN_W, (int)SCREEN_H, (Color){0, 0, 0, 90}, (Color){0, 0, 0, 200});
}

static void DrawSongRow(int i) {
    const float dist = (float)i - listOffset;
    const float ad = fabsf(dist);
    const float y = ROW_CENTER_Y + dist * ROW_STEP;
    if (y < -60.0f || y > SCREEN_H + 40.0f)
        return; // fora da tela

    const float sel = fmaxf(0.0f, 1.0f - ad);          /* 1 = selecionada, 0 = longe */
    const float x = ROW_X + fminf(ad, 3.0f) * ROW_INDENT;
    const float size = 36.0f + 10.0f * sel;
    const char* name = songs[shown[i]].name;

    if (sel > 0.01f) {
        const Rectangle pill = {x - 26.0f, y - 22.0f, ROW_MAX_W * (0.85f + 0.15f * sel), 60.0f};
        DrawRectangleRounded(pill, 0.5f, 12, Fade(ROW_BG, sel));
        DrawRectangle((int)pill.x, (int)(pill.y + 10.0f), 5, 40, Fade(ACCENT, sel));
    }

    const unsigned char alpha = (unsigned char)(255.0f * fmaxf(0.25f, 1.0f - ad * 0.16f));
    const Color main = {
        (unsigned char)(TEXT_IDLE.r + (255 - TEXT_IDLE.r) * sel),
        (unsigned char)(TEXT_IDLE.g + (255 - TEXT_IDLE.g) * sel),
        (unsigned char)(TEXT_IDLE.b + (255 - TEXT_IDLE.b) * sel),
        alpha
    };

    const Vector2 textPos = {x, y - size * 0.25f};
    DrawTextEx(mainFont, name, (Vector2){textPos.x + 2.0f, textPos.y + 3.0f}, size, 2.0f,
               (Color){0, 0, 0, (unsigned char)(160.0f * sel + 40.0f)});
    DrawTextEx(mainFont, name, textPos, size, 2.0f, main);
}

static void DrawIconCover(void) {
    const float pulse = 1.0f + 0.04f * (1.0f - selectPulse);   /* entra com leve zoom */
    const float side = COVER_SIZE * pulse;
    const Rectangle frame = {COVER_X + (COVER_SIZE - side) * 0.5f, COVER_Y + (COVER_SIZE - side) * 0.5f, side, side};

    DrawRectangleRounded((Rectangle){frame.x + 6.0f, frame.y + 8.0f, frame.width, frame.height}, 0.08f, 10, SHADOW);
    DrawRectangleRounded(frame, 0.08f, 10, ICON_EMPTY_BG);

    if (FreeplayIcon_IsLoaded()) {
        const Texture2D texture = FreeplayIcon_Texture();
        DrawTexturePro(texture, SquareCrop(texture), frame, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    } else {
        static const char* const missing = "NO ICON";
        static TextMeasure missingMeasure;
        const Vector2 size = TextMeasure_Get(&missingMeasure, mainFont, missing, 25.0f, 2.0f);
        DrawTextEx(mainFont, missing,
                   (Vector2){frame.x + (frame.width - size.x) * 0.5f, frame.y + (frame.height - size.y) * 0.5f},
                   25.0f, 2.0f, ICON_EMPTY_TEXT);
    }
    DrawRectangleRoundedLinesEx(frame, 0.08f, 10, 2.0f, Fade(ACCENT, 0.6f + 0.4f * selectPulse));
}

static void DrawSelectedCard(void) {
    DrawRectangleRounded((Rectangle){CARD_X + 6.0f, CARD_Y + 10.0f, CARD_W, CARD_H}, 0.05f, 10, SHADOW);
    DrawRectangleRounded((Rectangle){CARD_X, CARD_Y, CARD_W, CARD_H}, 0.05f, 10, CARD_BG);
    DrawRectangleRoundedLinesEx((Rectangle){CARD_X, CARD_Y, CARD_W, CARD_H}, 0.05f, 10, 1.5f, CARD_EDGE);

    DrawIconCover();

    // Titulo, centralizado; a fonte ja foi ajustada em SelectSong.
    const char* title = CurrentSong()->name;
    const float ty = COVER_Y + COVER_SIZE + 22.0f;
    DrawTextEx(mainFont, title, (Vector2){titleX + 2.0f, ty + 2.0f}, titleSize, 2.0f, (Color){0, 0, 0, 200});
    DrawTextEx(mainFont, title, (Vector2){titleX, ty}, titleSize, 2.0f, WHITE);

    // Oponente.
    const float oy = ty + 62.0f;
    DrawTextEx(mainFont, "OPPONENT", (Vector2){CARD_X + CARD_PAD, oy}, 17.0f, 2.0f, TEXT_DIM);
    DrawTextEx(mainFont, FreeplayIcon_Opponent(), (Vector2){CARD_X + CARD_PAD, oy + 22.0f}, 24.0f, 2.0f, ACCENT);

    // Contador e barra de progresso da lista.
    const float by = CARD_Y + CARD_H - 62.0f;
    DrawTextEx(mainFont, counterText, (Vector2){CARD_X + CARD_PAD, by}, 26.0f, 2.0f, CYAN);
    const float barW = CARD_W - 2.0f * CARD_PAD;
    const float frac = shownCount > 0 ? (float)(selectedSong + 1) / (float)shownCount : 0.0f;
    DrawRectangleRounded((Rectangle){CARD_X + CARD_PAD, by + 40.0f, barW, 6.0f}, 0.5f, 6, (Color){50, 44, 80, 255});
    DrawRectangleRounded((Rectangle){CARD_X + CARD_PAD, by + 40.0f, barW * frac, 6.0f}, 0.5f, 6, CYAN);
}

/* Tecla em "caixa" para a legenda de ajuda. */
static void DrawKeyHint(float x, float y, const char* key, const char* action) {
    static TextMeasure keyMeasure;
    const Vector2 k = TextMeasure_Get(&keyMeasure, mainFont, key, 16.0f, 1.0f);
    DrawRectangleRounded((Rectangle){x, y, k.x + 16.0f, 26.0f}, 0.3f, 6, (Color){255, 255, 255, 30});
    DrawTextEx(mainFont, key, (Vector2){x + 8.0f, y + 4.0f}, 16.0f, 1.0f, WHITE);
    DrawTextEx(mainFont, action, (Vector2){x + k.x + 24.0f, y + 5.0f}, 16.0f, 1.0f, TEXT_DIM);
}

static void Freeplay_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&cam);
    DrawBackdrop();

    if (songCount <= 0) {
        DrawTextEx(mainFont, "NO SONGS FOUND", (Vector2) {150.0f, 330.0f}, 42.0f, 3.0f, WHITE);
        DrawTextEx(mainFont, "Add songs to assets/songs/ or assets/mods/<mod>/songs/",
                   (Vector2) {150.0f, 390.0f}, 21.0f, 1.5f, (Color) {210, 210, 220, 255});
        Render_StopCamera();
        return;
    }

    if (shownCount <= 0) {
        DrawTextEx(mainFont, "NO MATCH", (Vector2) {150.0f, 330.0f}, 42.0f, 3.0f, WHITE);
        DrawTextEx(mainFont, "Try another name and press ENTER", (Vector2) {150.0f, 390.0f}, 21.0f, 1.5f,
                   (Color) {210, 210, 220, 255});
        SearchBox_Draw(&search, mainFont);
        Render_StopCamera();
        return;
    }

    // So as linhas perto da selecao; as demais nem sao percorridas.
    const int first = (int)fmaxf(0.0f, floorf(listOffset - ROW_VISIBLE));
    const int last = (int)fminf((float)(shownCount - 1), ceilf(listOffset + ROW_VISIBLE));
    for (int i = first; i <= last; ++i)
        DrawSongRow(i);

    DrawSelectedCard();

    SearchBox_Draw(&search, mainFont);

    // Ajuda no rodape, em caixas de tecla.
    DrawKeyHint(105.0f, 662.0f, "UP / DOWN", "escolher");
    DrawKeyHint(275.0f, 662.0f, "ENTER", "jogar");
    DrawKeyHint(370.0f, 662.0f, "ESC", "voltar");
    DrawKeyHint(470.0f, 662.0f, "CLICK SEARCH", "filtrar");

    Render_StopCamera();
}

static void Freeplay_Destroy([[maybe_unused]] RayScene* scene) {
    FreeplayIcon_Unload();
    bg.image = (Texture2D) {0}; // textura compartilhada: quem libera e MenuBackground_Free

    SongList_Free(songs);
    songs = NULL;
    songCount = 0;
    free(shown);
    shown = NULL;
    shownCount = 0;
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
