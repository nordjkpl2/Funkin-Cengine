/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenu.c - menu de opcoes (cena): ciclo de vida, entrada e desenho da lista.
 * Os valores ficam em GameData; a logica esta em OptionsMenuValues.c, OptionsMenuScreens.c e OptionsMenuIcons.c.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuBackground.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/options/OptionsMenu.h"
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "widgets-and-menus-ui/SearchBox.h"
#include "scenes/menu/MenuMusic.h"
#include <stdio.h>

float scrollOffset; // posicao suavizada da lista (em linhas)
float windowLogoX;  // calculado uma vez no Create
float otherLogoX;
float gameplayLogoX;
float visualLogoX;
float musicLogoX;
static Camera2D cam;
static RayGraphicObject bg;

/* X do logo ao lado de um cabecalho: logo depois do fim do texto (medido uma vez no Create). */
static float LogoXAfterHeader(int header) {
    const Vector2 size = MeasureTextEx(mainFont, options[header].text, HEADER_FONT_SIZE, TEXT_SPACING);
    return HEADER_X - TEXT_SHADOW + size.x + LOGO_MARGIN;
}

static void OptionsMenu_Create([[maybe_unused]] RayScene* scene) {
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = MenuBackground_Get();
    Render_DefaultCamera(&cam);

    windowLogoX = LogoXAfterHeader(OPTION_HEADER_WINDOW);
    otherLogoX = LogoXAfterHeader(OPTION_HEADER_OTHER);
    gameplayLogoX = LogoXAfterHeader(OPTION_HEADER_GAMEPLAY);
    visualLogoX = LogoXAfterHeader(OPTION_HEADER_VISUAL);
    musicLogoX = LogoXAfterHeader(OPTION_HEADER_MUSIC);

    SearchBox_Init(&search, (Rectangle) {SEARCH_X, SEARCH_Y, SEARCH_W, SEARCH_H});
    SearchBox_SetCompact(&search, true);

    Options_LoadValues();

    if (!Option_IsSelectable(currentOption))
        MoveSelection(1);
}

static void OptionsMenu_Update([[maybe_unused]] RayScene* scene) {
    if (capturing) {
        Capture_Update();
        return;
    }
    if (threadsScreen) {
        ThreadsScreen_Update();
        return;
    }
    if (noteColorScreen) {
        NoteColorScreen_Update();
        return;
    }

    // Lupa: le o clique nela e, com o campo aberto, o teclado (Esc/Backspace nao saem da cena).
    const bool typing = SearchBox_Focused(&search);
    if (SearchBox_Update(&search)) {
        layoutDirty = true;               // filtro mudou: refaz o layout
        if (!Option_IsSelectable(currentOption))
            MoveSelection(1);             // garante uma linha selecionavel
    }
    if (typing || SearchBox_Focused(&search))
        return;

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        if (MusicAutoOn()) MenuMusic_RestoreSaved();   // Auto nao fica tocando fora da aba
        GameData_Flush();
        MenuState_SetScene();
        return;
    }

    // Auto: so avanca quando a faixa atual acaba (stream sem loop).
    if (MusicAutoOn() && MenuMusic_Count() > 0 && RayGame_MusicFinished()) {
        autoTrack = (autoTrack + 1) % MenuMusic_Count();
        MenuMusic_Play(autoTrack, 0);
        RefreshAutoLabel();
    }

    scrollOffset = Lerp(scrollOffset, (float)Option_VisualIndex(currentOption), RayGame_DeltaTime() * SCROLL_LERP_SPEED);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) MoveSelection(1);
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) MoveSelection(-1);

    // Roda do mouse: acumula o movimento (touchpad manda fracoes) e pula varias linhas por clique.
    {
        static float wheelAcc;
        wheelAcc += GetMouseWheelMove();
        while (wheelAcc >= 1.0f) {
            wheelAcc -= 1.0f;
            for (int k = 0; k < WHEEL_ROWS_PER_NOTCH; k++) MoveSelection(-1);
        }
        while (wheelAcc <= -1.0f) {
            wheelAcc += 1.0f;
            for (int k = 0; k < WHEEL_ROWS_PER_NOTCH; k++) MoveSelection(1);
        }
    }

    Option* cur = options + currentOption;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (cur->type == OPTION_VALUE_BOOL) {
            cur->value = !cur->value;
            ApplyBoolValue();
        }
        else if (cur->type == OPTION_VALUE_HEADER) {
            cur->collapsed = !cur->collapsed;
            layoutDirty = true;
        }
        else if (currentOption == OPTION_KEYBOARD_SCHEME && (int)cur->value == SCHEME_CUSTOM) {
            Capture_Begin();
        }
        else if (currentOption == OPTION_THREADS && (int)cur->value == 1) {
            ThreadsScreen_Begin();
        }
        else if (currentOption == OPTION_NOTE_COLORS) {
            NoteColorScreen_Begin();
        }
    }
    else if (cur->type != OPTION_VALUE_BOOL && cur->type != OPTION_VALUE_HEADER) {
        const float step = IsKeyDown(KEY_LEFT_ALT) ? cur->altStep : cur->step;

        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            cur->value -= step;
            ApplyNumValue();
        }
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            cur->value += step;
            ApplyNumValue();
        }
    }
}

static void OptionsMenu_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);

    float y = SCREEN_H / 2.0f - scrollOffset * ROW_HEIGHT;

    for (int i = 0; i < OPTION_COUNT; i++) {
        if (!Option_IsVisible(i))
            continue; // secao recolhida: nao ocupa espaco

        Option* o = options + i;
        const bool isHeader = o->type == OPTION_VALUE_HEADER;
        const float height = isHeader ? HEADER_HEIGHT : ROW_HEIGHT;
        const float drawY = y;
        y += height;

        if (drawY > CULL_BOTTOM)
            break; // as linhas descem em ordem: o resto tambem esta fora da tela
        if (drawY < CULL_TOP)
            continue;

        const bool selected = currentOption == i;

        if (isHeader) {
            Color fill = YELLOW, shadow = BLACK;
            fill.a = selected ? 255 : HEADER_DIM_ALPHA;
            shadow.a = selected ? 255 : 100;

            DrawOptionText(o->text, HEADER_X, drawY, HEADER_FONT_SIZE, fill, shadow);
            DrawChevron(drawY, o->collapsed, fill, shadow);

            if (i == OPTION_HEADER_WINDOW)
                DrawWindowLogo(drawY, fill, shadow);
            else if (i == OPTION_HEADER_GAMEPLAY)
                DrawGameplayLogo(drawY, fill, shadow);
            else if (i == OPTION_HEADER_VISUAL)
                DrawVisualLogo(drawY, fill, shadow);
            else if (i == OPTION_HEADER_OTHER)
                DrawOtherLogo(drawY, fill, shadow);
            else if (i == OPTION_HEADER_MUSIC)
                DrawMusicLogo(drawY, fill, shadow);
            continue;
        }

        if (o->dirty) Option_RebuildText(o);

        Color fill = o->active ? WHITE : RED;
        Color shadow = BLACK;
        fill.a = selected ? 255 : 150;
        shadow.a = selected ? 255 : 100;

        // Framerate ate V-SYNC (secao Window): icone de TV e texto recuado.
        const bool inWindow = i >= OPTION_FPS && i <= OPTION_VSYNC;
        const float textX = inWindow ? ROW_X - WINDOW_ROW_SHIFT : ROW_X;
        DrawOptionText(o->cachedText, textX, drawY, ROW_FONT_SIZE, fill, shadow);
        if (inWindow) DrawTvIcon(drawY, fill, shadow);

        const bool isHealthDrain = i == OPTION_HEALTH_DRAIN_PLAY || i == OPTION_HEALTH_DRAIN_BOTPLAY;
        if (isHealthDrain) DrawHealthIcons(drawY, fill, shadow);

        const bool hasKeyArrows = i == OPTION_KEYBOARD_SCHEME || i == OPTION_SCROLL_STYLE || i == OPTION_GHOST;
        if (hasKeyArrows) DrawKeyArrow(drawY, fill, shadow);

        DrawExtraIcon(i, drawY, fill, shadow);
    }

    if (currentOption == OPTION_KEYBOARD_SCHEME && (int)options[OPTION_KEYBOARD_SCHEME].value == SCHEME_CUSTOM)
        DrawTextEx(mainFont, "ENTER: edit custom keys", (Vector2) {ROW_X, SCREEN_H - 50.0f}, 24, 2, (Color) {245, 220, 120, 255});

    if (currentOption == OPTION_NOTE_COLORS)
        DrawTextEx(mainFont, "ENTER: edit note colors", (Vector2) {ROW_X, SCREEN_H - 50.0f}, 24, 2, (Color) {245, 220, 120, 255});

    if (currentOption == OPTION_THREADS && (int)options[OPTION_THREADS].value == 1)
        DrawTextEx(mainFont, "ENTER: set thread count", (Vector2) {ROW_X, SCREEN_H - 50.0f}, 24, 2, (Color) {245, 220, 120, 255});

    SearchBox_Draw(&search, mainFont);

    if (capturing)
        Capture_Draw();
    else if (threadsScreen)
        ThreadsScreen_Draw();
    else if (noteColorScreen)
        NoteColorScreen_Draw();

    Render_StopCamera();
}

static void OptionsMenu_Destroy([[maybe_unused]] RayScene* scene) {
    NoteColorScreen_Destroy();
    bg.image = (Texture2D) {0}; // compartilhada: nao descarrega aqui
}

static RayScene scene;
Scene_MakeSceneCode(scene, OptionsMenu_SetScene, OptionsMenu_Create, OptionsMenu_Update, OptionsMenu_Draw, OptionsMenu_Destroy)
