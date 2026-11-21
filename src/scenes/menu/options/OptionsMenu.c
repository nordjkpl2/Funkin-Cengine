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
#include "rendering/TextMeasure.h"
#include <stdio.h>
#include <string.h>

float scrollOffset; // posicao suavizada da lista (em linhas)
float windowLogoX;  // calculado uma vez no Create
float otherLogoX;
float gameplayLogoX;
float visualLogoX;
float musicLogoX;
float renderLogoX;
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
    renderLogoX = LogoXAfterHeader(OPTION_HEADER_RENDER);

    SearchBox_Init(&search, (Rectangle) {SEARCH_X, SEARCH_Y, SEARCH_W, SEARCH_H});
    SearchBox_SetCompact(&search, true);

    Options_LoadValues();

    if (!Option_IsSelectable(currentOption))
        MoveSelection(1);
}

#define ROW_CLICK_W 1280.0f // largura clicavel da linha (a tela inteira)

/* Reset pede confirmacao: o primeiro ENTER arma, o segundo executa. Mover a selecao desarma. */
static bool resetArmed;

/* Acao de ENTER/clique na opcao atual: liga/desliga, recolhe secao ou abre a tela da opcao. */
static void ActivateCurrent(void) {
    Option* cur = options + currentOption;

    if (cur->type == OPTION_VALUE_ACTION) {
        if (!resetArmed) { resetArmed = true; return; }
        resetArmed = false;
        Options_ResetToDefault();
    }
    else if (cur->type == OPTION_VALUE_BOOL) {
        cur->value = !cur->value;
        ApplyBoolValue();
    }
    else if (cur->type == OPTION_VALUE_HEADER) {
        if (!Option_HorizontalMode()) {   // no menu horizontal o titulo so abre a aba (ja aberta ao selecionar)
            cur->collapsed = !cur->collapsed;
            layoutDirty = true;
        }
    }
    else if (currentOption == OPTION_KEYBOARD_SCHEME && (int)cur->value == SCHEME_CUSTOM)
        Capture_Begin();
    else if (currentOption == OPTION_THREADS && (int)cur->value == 1)
        ThreadsScreen_Begin();
    else if (currentOption == OPTION_NOTE_COLORS)
        NoteColorScreen_Begin();
}

/* Icone do titulo (o mesmo da lista) em cima do nome, alinhado a esquerda do titulo. */
static void DrawTabLogo(int header, float x, float rowY, Color fill, Color shadow) {
    const float saved[6] = {windowLogoX, otherLogoX, gameplayLogoX, visualLogoX, musicLogoX, renderLogoX};
    windowLogoX = otherLogoX = gameplayLogoX = visualLogoX = musicLogoX = renderLogoX = x;

    if (header == OPTION_HEADER_WINDOW) DrawWindowLogo(rowY, fill, shadow);
    else if (header == OPTION_HEADER_GAMEPLAY) DrawGameplayLogo(rowY, fill, shadow);
    else if (header == OPTION_HEADER_VISUAL) DrawVisualLogo(rowY, fill, shadow);
    else if (header == OPTION_HEADER_OTHER) DrawOtherLogo(rowY, fill, shadow);
    else if (header == OPTION_HEADER_MUSIC) DrawMusicLogo(rowY, fill, shadow);
    else if (header == OPTION_HEADER_RENDER) DrawRenderLogo(rowY, fill, shadow);

    windowLogoX = saved[0]; otherLogoX = saved[1]; gameplayLogoX = saved[2]; visualLogoX = saved[3]; musicLogoX = saved[4]; renderLogoX = saved[5];
}

/* Largura do icone de cada aba (mesmas medidas de OptionsMenuIcons.c), para a hitbox cobrir o icone todo. */
static float TabIconWidth(int header) {
    switch (header) {
        case OPTION_HEADER_WINDOW:   return 2.0f * LOGO_SQUARE + LOGO_GAP;
        case OPTION_HEADER_GAMEPLAY: return 3.0f * (2.0f * SYMBOL_HALF + SYMBOL_GAP) + 2.0f * SYMBOL_HALF;
        case OPTION_HEADER_VISUAL:   return FRAME_WIDTH;
        case OPTION_HEADER_OTHER:    return 2.0f * LOGO_CIRCLE_RADIUS;
        case OPTION_HEADER_RENDER:   return 2.0f * GEAR_OUTER_R;
        case OPTION_HEADER_MUSIC:    return 2.0f * NOTE_HEAD_RADIUS + NOTE_FLAG_W;
        default:                     return 0.0f;
    }
}

/* Barra de titulos do menu horizontal. Com draw desenha; senao so devolve o titulo sob o ponto m (ou -1). */
static int TabBarRun(bool draw, Vector2 m) {
    static TextMeasure tabMeasure[OPTION_COUNT]; // a barra e desenhada todo frame; o titulo de cada aba e fixo
    int hit = -1;
    float x = TAB_X;
    bool first = true;

    for (int i = 0; i < OPTION_COUNT; i++) {
        if (options[i].type != OPTION_VALUE_HEADER) continue;

        if (!first) {   // separador "|" entre os titulos
            if (draw) DrawTextEx(mainFont, "|", (Vector2) {x + TAB_GAP * 0.5f - 6.0f, TAB_BAR_Y}, TAB_FONT_SIZE, TEXT_SPACING, GRAY);
            x += TAB_GAP;
        }
        first = false;

        // "[Gameplay]" -> "Gameplay"
        const char* src = options[i].text;
        size_t len = strlen(src);
        if (src[0] == '[') { src++; len--; }
        if (len > 0 && src[len - 1] == ']') len--;
        char label[64];
        (void)snprintf(label, sizeof(label), "%.*s", (int)len, src);

        const Vector2 size = TextMeasure_Get(&tabMeasure[i], mainFont, label, TAB_FONT_SIZE, TEXT_SPACING);
        if (!draw) {
            // Uma hitbox so: o retangulo que o icone + titulo ocupam (largura do maior dos dois).
            const float boxW = size.x > TabIconWidth(i) ? size.x : TabIconWidth(i);
            const float boxX = x - TEXT_SHADOW;
            const float boxY = TAB_BAR_Y - TAB_LOGO_LIFT;
            const float boxH = TAB_LOGO_LIFT + TAB_FONT_SIZE;
            if (m.x >= boxX && m.x < boxX + boxW && m.y >= boxY && m.y < boxY + boxH)
                hit = i;
        } else {
            const bool open = options[currentOption].header == i;   // aba aberta
            Color fill = YELLOW, shadow = BLACK;
            fill.a = open ? 255 : HEADER_DIM_ALPHA;
            shadow.a = open ? 255 : 100;
            DrawOptionText(label, x, TAB_BAR_Y, TAB_FONT_SIZE, fill, shadow);
            DrawTabLogo(i, x, TAB_BAR_Y - TAB_LOGO_LIFT, fill, shadow);
        }
        x += size.x;
    }
    return hit;
}

/* Indice da linha sob o ponto m (mesma conta de y que o Draw), ou -1. So roda no clique. */
static int RowAtPoint(Vector2 m) {
    float y = SCREEN_H / 2.0f - scrollOffset * ROW_HEIGHT;
    if (m.x < 0.0f || m.x >= ROW_CLICK_W) return -1;

    for (int i = 0; i < OPTION_COUNT; i++) {
        if (!Option_IsVisible(i)) continue;
        const float height = options[i].type == OPTION_VALUE_HEADER ? HEADER_HEIGHT : ROW_HEIGHT;
        if (m.y >= y && m.y < y + height) return i;
        y += height;
    }
    return -1;
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
    if (currentOption != OPTION_RESET_CONFIG) resetArmed = false;

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

    // Clique esquerdo: seleciona a linha e faz o mesmo que ENTER nela.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = RayGame_MouseWorld();
        if (Option_HorizontalMode()) {
            const int tab = TabBarRun(false, mouse);
            if (tab >= 0) { currentOption = tab; return; }
        }
        const int row = RowAtPoint(mouse);
        if (row >= 0 && Option_IsSelectable(row)) {
            currentOption = row;
            ActivateCurrent();
            return;
        }
    }

    Option* cur = options + currentOption;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        ActivateCurrent();
    }
    else if (cur->type == OPTION_VALUE_HEADER) {
        if (Option_HorizontalMode()) {   // titulo selecionado: Left/Right troca de aba
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) Tab_Step(-1);
            else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) Tab_Step(1);
        }
    }
    else if (cur->type != OPTION_VALUE_BOOL) {
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

static void DrawHint(const char* text) {
    const Vector2 pos = {ROW_X, SCREEN_H - 50.0f};
    DrawTextEx(mainFont, text, (Vector2) {pos.x + 2.0f, pos.y + 2.0f}, 24, 2, (Color) {0, 0, 0, 200});   // sombra
    DrawTextEx(mainFont, text, pos, 24, 2, (Color) {245, 220, 120, 255});
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
            else if (i == OPTION_HEADER_RENDER)
                DrawRenderLogo(drawY, fill, shadow);
            continue;
        }

        if (o->type == OPTION_VALUE_LABEL) {
            // Texto estatico: cinza, sem icones, nunca selecionado.
            // Fonte menor, centralizada na altura da linha (o layout continua o mesmo).
            const float labelY = drawY + (ROW_FONT_SIZE - LABEL_FONT_SIZE) * 0.5f;
            DrawOptionText(o->text, ROW_X, labelY, LABEL_FONT_SIZE, (Color) {190, 190, 190, 255}, (Color) {0, 0, 0, 100});
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

        const bool hasKeyArrows = i == OPTION_KEYBOARD_SCHEME || i == OPTION_GHOST;
        if (hasKeyArrows) DrawKeyArrow(drawY, fill, shadow);

        DrawExtraIcon(i, drawY, fill, shadow);
    }

    if (Option_HorizontalMode()) {
        DrawRectangle(0, 0, 1280, (int)TAB_BAND_H, (Color) {0, 0, 0, 180});   // faixa da barra (a lista passa por baixo)
        TabBarRun(true, (Vector2) {0.0f, 0.0f});
    }

    // Dica da opcao atual, no rodape.
    if (currentOption == OPTION_KEYBOARD_SCHEME && (int)options[OPTION_KEYBOARD_SCHEME].value == SCHEME_CUSTOM)
        DrawHint("ENTER: edit custom keys");
    else if (currentOption == OPTION_NOTE_COLORS)
        DrawHint("ENTER: edit note colors");
    else if (currentOption == OPTION_THREADS && (int)options[OPTION_THREADS].value == 1)
        DrawHint("ENTER: set thread count");
    else if (currentOption == OPTION_RESET_CONFIG)
        DrawHint(resetArmed ? "ENTER again to confirm: resets every option" : "ENTER: reset all options to default");

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
