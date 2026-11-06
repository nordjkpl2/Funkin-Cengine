/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenu.c - menu de opcoes (cena). Os valores salvos ficam em GameData.
 */
#include "scenes/AllScenes.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"

#define SCREEN_H 720.0f

#define SCROLL_LERP_SPEED 10.0f
#define ROW_FONT_SIZE 70
#define ROW_PADDING 30
#define HEADER_FONT_SIZE 90
#define HEADER_PADDING 10
#define TEXT_SHADOW 5
#define TEXT_SPACING 5.0f
#define ROW_X 150.0f
#define HEADER_X 50.0f
#define ROW_HEIGHT (ROW_FONT_SIZE + ROW_PADDING)
#define HEADER_HEIGHT (HEADER_FONT_SIZE + HEADER_PADDING)
#define CULL_TOP -100.0f
#define CULL_BOTTOM 850.0f

// Logo de 4 quadrados ao lado de "Window Options".
#define LOGO_SQUARE 30.0f
#define LOGO_GAP 5.0f
#define LOGO_MARGIN 25.0f // distancia do fim do texto
#define LOGO_TOP 13.0f    // distancia do topo da linha

// Setinha de secao (aberta = para baixo, recolhida = para a direita).
#define CHEVRON_X 22.0f
#define CHEVRON_SIZE 9.0f
#define CHEVRON_STROKE 5.0f
#define HEADER_DIM_ALPHA 170
#define LOGO_CIRCLE_RADIUS 32.0f // circulo ao lado de "Other Options"

// Simbolos do PlayStation ao lado de "Gameplay Options": X, quadrado, triangulo, bolinha.
#define SYMBOL_HALF 24.0f    // metade do tamanho de cada simbolo
#define SYMBOL_STROKE 7.0f   // espessura do traco
#define SYMBOL_GAP 22.0f     // espaco entre simbolos

typedef enum {
    OPTION_VALUE_INT,
    OPTION_VALUE_FLOAT,
    OPTION_VALUE_BOOL,
    OPTION_VALUE_LIST,
    OPTION_VALUE_HEADER
} OptionValueType;

typedef struct {
    char* text;
    OptionValueType type;

    const char** options; // nomes, quando type == LIST
    float value, min, max, step, altStep;
    char active; // inativa = vermelha e pulada na navegacao

    char cachedText[96]; // texto pronto: nao formata a cada frame
    bool dirty;

    int header;     // indice do cabecalho da secao a que a opcao pertence
    bool collapsed; // so nos cabecalhos: secao recolhida
} Option;

typedef enum {
    OPTION_HEADER_WINDOW,
    OPTION_FPS,
    OPTION_UNCAPPED_FPS,
    OPTION_SHOW_FPS,
    OPTION_VSYNC,
    OPTION_HEADER_GAMEPLAY,
    OPTION_KEYBOARD_SCHEME,
    OPTION_SCROLL_STYLE,
    OPTION_GHOST,
    OPTION_HITSOUND,
    OPTION_HEADER_OTHER,
    OPTION_SHOW_SCORE,
    OPTION_BOPPING_ICONS,
    OPTION_ZOOMFACTOR_UI,
    OPTION_ZOOMFACTOR_GAME,
    OPTION_FILL_SCREEN,
    OPTION_BOTPLAY,
    OPTION_COUNT
} OptionOrder;

static const char* ghost_names[3] = {"On", "50/50", "Off"};
static const char* scroll_names[3] = {"Normal", "Downscroll", "Middlescroll"};

static Option options[OPTION_COUNT] = {
    [OPTION_HEADER_WINDOW] = {.text = "[ Window Options ]", .type = OPTION_VALUE_HEADER},
    [OPTION_FPS] = {.text = "Framerate", .type = OPTION_VALUE_INT, .min = 60, .max = 2000, .step = 10, .altStep = 100},
    [OPTION_UNCAPPED_FPS] = {.text = "Uncapped FPS", .type = OPTION_VALUE_BOOL},
    [OPTION_SHOW_FPS] = {.text = "Show FPS", .type = OPTION_VALUE_BOOL},
    [OPTION_VSYNC] = {.text = "V-SYNC", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_GAMEPLAY] = {.text = "[ Gameplay Options ]", .type = OPTION_VALUE_HEADER},
    [OPTION_KEYBOARD_SCHEME] = {.text = "Scheme", .type = OPTION_VALUE_LIST, .options = game_available_schemes, .step = 1, .max = 3, .min = 0},
    [OPTION_SCROLL_STYLE] = {.text = "Scroll Style", .type = OPTION_VALUE_LIST, .options = scroll_names, .step = 1, .max = 2, .min = 0},
    [OPTION_GHOST] = {.text = "Ghost Tapping", .type = OPTION_VALUE_LIST, .options = ghost_names, .step = 1, .max = 2, .min = 0},
    [OPTION_HITSOUND] = {.text = "Hit Sound", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_OTHER] = {.text = "[ Other Options ]", .type = OPTION_VALUE_HEADER},
    [OPTION_SHOW_SCORE] = {.text = "Show Score", .type = OPTION_VALUE_BOOL},
    [OPTION_BOPPING_ICONS] = {.text = "Bopping Icons", .type = OPTION_VALUE_BOOL},
    [OPTION_ZOOMFACTOR_UI] = {.text = "UI beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_ZOOMFACTOR_GAME] = {.text = "Game beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_FILL_SCREEN] = {.text = "Stretch", .type = OPTION_VALUE_BOOL},
    [OPTION_BOTPLAY] = {.text = "Botplay", .type = OPTION_VALUE_BOOL},
};

static float scrollOffset; // posicao suavizada da lista (em linhas)
static float windowLogoX;  // calculado uma vez no Create
static float otherLogoX;
static float gameplayLogoX;
static int currentOption = OPTION_FPS; // comeca na primeira opcao, nao no cabecalho
static Camera2D cam;
static RayGraphicObject bg;

/* ------------------------------------------------------------------ */
/* Texto das opcoes                                                    */
/* ------------------------------------------------------------------ */

static void Option_RebuildText(Option* o) {
    if (o->type == OPTION_VALUE_HEADER) return;

    char* out = o->cachedText;
    const size_t size = sizeof(o->cachedText);

    if (!o->active)
        (void)snprintf(out, size, "%s", o->text);
    else switch (o->type) {
        case OPTION_VALUE_INT:   (void)snprintf(out, size, "%s: < %d >", o->text, (int)o->value); break;
        case OPTION_VALUE_FLOAT: (void)snprintf(out, size, "%s: < %.2f >", o->text, (double)o->value); break;
        case OPTION_VALUE_BOOL:  (void)snprintf(out, size, "%s: [ %s ]", o->text, o->value ? "ON" : "OFF"); break;
        case OPTION_VALUE_LIST:  (void)snprintf(out, size, "%s: << %s >>", o->text, o->options[(int)o->value]); break;
        default: break;
    }

    o->dirty = false;
}

/* ------------------------------------------------------------------ */
/* Aplicar mudancas de valor                                           */
/* ------------------------------------------------------------------ */

/* Valor numerico/lista mudou: limita, aplica no jogo e refaz o texto. */
static void ApplyNumValue(void) {
    Option* o = options + currentOption;
    o->value = Clamp(o->value, o->min, o->max);

    switch (currentOption) {
        case OPTION_FPS:
            // Pula a faixa 500-900 (de 500 vai direto a 900 e vice-versa).
            if (o->value > 500 && o->value < 900) o->value = (o->value < 700) ? 900 : 500;
            RayGame_SetFPS((int)o->value);
            break;
        case OPTION_KEYBOARD_SCHEME:   game_options_scheme = (int)o->value; break;
        case OPTION_SCROLL_STYLE:      game_options_scroll = (char)(int)o->value; break;
        case OPTION_GHOST:             game_options_ghost = (char)(int)o->value; break;
        case OPTION_ZOOMFACTOR_GAME:   game_options_zoomFactorGAME = o->value; break;
        case OPTION_ZOOMFACTOR_UI:     game_options_zoomFactorUI = o->value; break;
    }

    Option_RebuildText(o);
}

/* Liga/desliga uma opcao booleana ja alterada e aplica no jogo. */
static void ApplyBoolValue(void) {
    Option* o = options + currentOption;
    const char on = (char)o->value;

    switch (currentOption) {
        case OPTION_UNCAPPED_FPS:
            RayGame_SetFPS(on ? 0 : (int)options[OPTION_FPS].value);
            options[OPTION_FPS].active = !on;
            options[OPTION_FPS].dirty = true;
            Option_RebuildText(&options[OPTION_FPS]);
            break;
        case OPTION_VSYNC:
            RayGame_SetVsync(on);
            options[OPTION_UNCAPPED_FPS].active = !on;
            options[OPTION_SHOW_FPS].active = 1;
            options[OPTION_FPS].active = !on && !options[OPTION_UNCAPPED_FPS].value;
            // Estado ativo/inativo mudou: o Draw refaz o texto dessas linhas.
            options[OPTION_FPS].dirty = true;
            options[OPTION_UNCAPPED_FPS].dirty = true;
            options[OPTION_SHOW_FPS].dirty = true;
            break;
        case OPTION_SHOW_FPS:      game_options_showFps = on; break;
        case OPTION_SHOW_SCORE:    game_options_showScore = on; break;
        case OPTION_BOPPING_ICONS: game_options_bopIcons = on; break;
        case OPTION_FILL_SCREEN:   game_options_fillScreen = on; break;
        case OPTION_BOTPLAY:       game_options_botplay = on; break;
        case OPTION_HITSOUND:      game_options_hitSound = on; break;
    }

    Option_RebuildText(o);
}

/* ------------------------------------------------------------------ */
/* Navegacao                                                           */
/* ------------------------------------------------------------------ */

/* Cabecalhos sempre aparecem; opcoes somem quando a secao esta recolhida. */
static bool Option_IsVisible(int i) {
    return options[i].type == OPTION_VALUE_HEADER || !options[options[i].header].collapsed;
}

/* Cabecalhos sao selecionaveis (para abrir/recolher); opcoes, so se visiveis e ativas. */
static bool Option_IsSelectable(int i) {
    if (options[i].type == OPTION_VALUE_HEADER) return true;
    return Option_IsVisible(i) && options[i].active;
}

/* Posicao da opcao na tela, em linhas (ignora as recolhidas). */
static int Option_VisualIndex(int index) {
    int visual = 0;
    for (int i = 0; i < index; i++)
        if (Option_IsVisible(i)) visual++;
    return visual;
}

/* Anda para a proxima linha selecionavel. */
static void MoveSelection(int dir) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        currentOption = (currentOption + dir + OPTION_COUNT) % OPTION_COUNT;
        if (Option_IsSelectable(currentOption)) break;
    }
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

static void OptionsMenu_Create([[maybe_unused]] RayScene* scene) {
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");
    Render_DefaultCamera(&cam);

    const Vector2 headerSize = MeasureTextEx(mainFont, options[OPTION_HEADER_WINDOW].text, HEADER_FONT_SIZE, TEXT_SPACING);
    windowLogoX = HEADER_X - TEXT_SHADOW + headerSize.x + LOGO_MARGIN;

    const Vector2 otherSize = MeasureTextEx(mainFont, options[OPTION_HEADER_OTHER].text, HEADER_FONT_SIZE, TEXT_SPACING);
    otherLogoX = HEADER_X - TEXT_SHADOW + otherSize.x + LOGO_MARGIN;

    const Vector2 gameplaySize = MeasureTextEx(mainFont, options[OPTION_HEADER_GAMEPLAY].text, HEADER_FONT_SIZE, TEXT_SPACING);
    gameplayLogoX = HEADER_X - TEXT_SHADOW + gameplaySize.x + LOGO_MARGIN;

    // Cada opcao guarda o cabecalho da sua secao; todas as secoes comecam abertas.
    int header = 0;
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (options[i].type == OPTION_VALUE_HEADER) header = i;
        options[i].header = header;
        options[i].collapsed = false;
        options[i].active = 1;
        options[i].dirty = true;
    }

    const int targetFps = RayGame_GetTargetFPS();
    options[OPTION_FPS].value = targetFps;
    if (targetFps == 0) {
        options[OPTION_FPS].active = 0;
        options[OPTION_FPS].value = 120;
    }

    options[OPTION_UNCAPPED_FPS].value = targetFps == 0;
    options[OPTION_SHOW_FPS].value = game_options_showFps;
    options[OPTION_SHOW_SCORE].value = game_options_showScore;
    options[OPTION_BOPPING_ICONS].value = game_options_bopIcons;
    options[OPTION_VSYNC].value = game_options_vsync;
    options[OPTION_KEYBOARD_SCHEME].value = game_options_scheme;
    options[OPTION_SCROLL_STYLE].value = game_options_scroll;
    options[OPTION_GHOST].value = game_options_ghost;
    options[OPTION_HITSOUND].value = game_options_hitSound;
    options[OPTION_ZOOMFACTOR_GAME].value = game_options_zoomFactorGAME;
    options[OPTION_ZOOMFACTOR_UI].value = game_options_zoomFactorUI;
    options[OPTION_FILL_SCREEN].value = game_options_fillScreen;
    options[OPTION_BOTPLAY].value = game_options_botplay;

    for (int i = 0; i < OPTION_COUNT; i++)
        Option_RebuildText(options + i);

    if (!Option_IsSelectable(currentOption))
        MoveSelection(1);
}

static void OptionsMenu_Update([[maybe_unused]] RayScene* scene) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        GameData_Flush();
        MenuState_SetScene();
        return;
    }

    scrollOffset = Lerp(scrollOffset, (float)Option_VisualIndex(currentOption), RayGame_DeltaTime() * SCROLL_LERP_SPEED);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) MoveSelection(1);
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) MoveSelection(-1);

    Option* cur = options + currentOption;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (cur->type == OPTION_VALUE_BOOL) {
            cur->value = !cur->value;
            ApplyBoolValue();
        }
        else if (cur->type == OPTION_VALUE_HEADER) {
            cur->collapsed = !cur->collapsed;
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

/* Texto com sombra deslocada para cima/esquerda. */
static void DrawOptionText(const char* text, float x, float y, int fontSize, Color fill, Color shadow) {
    DrawTextEx(mainFont, text, (Vector2) {x, y + TEXT_SHADOW}, fontSize, TEXT_SPACING, shadow);
    DrawTextEx(mainFont, text, (Vector2) {x - TEXT_SHADOW, y}, fontSize, TEXT_SPACING, fill);
}

/* Quatro quadrados (estilo logo do Windows), com a mesma sombra do texto. */
static void DrawWindowLogo(float rowY, Color fill, Color shadow) {
    const float step = LOGO_SQUARE + LOGO_GAP;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;

        for (int i = 0; i < 4; i++) {
            const float x = windowLogoX + (i % 2) * step + shift;
            const float y = rowY + LOGO_TOP + (i / 2) * step + shift;
            DrawRectangleRec((Rectangle) {x, y, LOGO_SQUARE, LOGO_SQUARE}, color);
        }
    }
}

/* Circulo com a mesma sombra do texto. */
static void DrawOtherLogo(float rowY, Color fill, Color shadow) {
    const float centerY = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f; // alinhado ao meio do logo de quadrados
    const float centerX = otherLogoX + LOGO_CIRCLE_RADIUS;

    DrawCircleV((Vector2) {centerX + TEXT_SHADOW, centerY + TEXT_SHADOW}, LOGO_CIRCLE_RADIUS, shadow);
    DrawCircleV((Vector2) {centerX, centerY}, LOGO_CIRCLE_RADIUS, fill);
}

/* Linha com pontas arredondadas. */
static void DrawRoundLine(Vector2 a, Vector2 b, Color color) {
    DrawLineEx(a, b, SYMBOL_STROKE, color);
    DrawCircleV(a, SYMBOL_STROKE * 0.5f, color);
    DrawCircleV(b, SYMBOL_STROKE * 0.5f, color);
}

/* X, quadrado, triangulo e bolinha em uma cor, deslocados por `shift`. */
static void DrawPlayStationSymbols(float rowY, float shift, Color color) {
    const float h = SYMBOL_HALF;
    const float step = 2.0f * h + SYMBOL_GAP;
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f + shift;
    float cx = gameplayLogoX + h + shift;

    // X
    DrawRoundLine((Vector2) {cx - h, cy - h}, (Vector2) {cx + h, cy + h}, color);
    DrawRoundLine((Vector2) {cx - h, cy + h}, (Vector2) {cx + h, cy - h}, color);

    // Quadrado
    cx += step;
    DrawRoundLine((Vector2) {cx - h, cy - h}, (Vector2) {cx + h, cy - h}, color);
    DrawRoundLine((Vector2) {cx + h, cy - h}, (Vector2) {cx + h, cy + h}, color);
    DrawRoundLine((Vector2) {cx + h, cy + h}, (Vector2) {cx - h, cy + h}, color);
    DrawRoundLine((Vector2) {cx - h, cy + h}, (Vector2) {cx - h, cy - h}, color);

    // Triangulo
    cx += step;
    const Vector2 top = {cx, cy - h - 2.0f};
    const Vector2 left = {cx - h - 2.0f, cy + h - 2.0f};
    const Vector2 right = {cx + h + 2.0f, cy + h - 2.0f};
    DrawRoundLine(top, left, color);
    DrawRoundLine(left, right, color);
    DrawRoundLine(right, top, color);

    // Bolinha
    cx += step;
    DrawRing((Vector2) {cx, cy}, h + 2.0f - SYMBOL_STROKE * 0.5f, h + 2.0f + SYMBOL_STROKE * 0.5f, 0.0f, 360.0f, 36, color);
}

static void DrawGameplayLogo(float rowY, Color fill, Color shadow) {
    DrawPlayStationSymbols(rowY, TEXT_SHADOW, shadow);
    DrawPlayStationSymbols(rowY, 0.0f, fill);
}

/* Setinha da secao: "v" aberta, ">" recolhida. */
static void DrawChevron(float rowY, bool collapsed, Color fill, Color shadow) {
    const float s = CHEVRON_SIZE;
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const float shift = pass == 0 ? TEXT_SHADOW * 0.6f : 0.0f;
        const Color color = pass == 0 ? shadow : fill;
        const float cx = CHEVRON_X + shift;
        const float y = cy + shift;

        Vector2 a, b, c;
        if (collapsed) {
            a = (Vector2) {cx - s * 0.55f, y - s};
            b = (Vector2) {cx + s * 0.55f, y};
            c = (Vector2) {cx - s * 0.55f, y + s};
        } else {
            a = (Vector2) {cx - s, y - s * 0.55f};
            b = (Vector2) {cx, y + s * 0.55f};
            c = (Vector2) {cx + s, y - s * 0.55f};
        }

        DrawLineEx(a, b, CHEVRON_STROKE, color);
        DrawLineEx(b, c, CHEVRON_STROKE, color);
        DrawCircleV(a, CHEVRON_STROKE * 0.5f, color);
        DrawCircleV(b, CHEVRON_STROKE * 0.5f, color);
        DrawCircleV(c, CHEVRON_STROKE * 0.5f, color);
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
            else if (i == OPTION_HEADER_OTHER)
                DrawOtherLogo(drawY, fill, shadow);
            continue;
        }

        if (o->dirty) Option_RebuildText(o);

        Color fill = o->active ? WHITE : RED;
        Color shadow = BLACK;
        fill.a = selected ? 255 : 150;
        shadow.a = selected ? 255 : 100;

        DrawOptionText(o->cachedText, ROW_X, drawY, ROW_FONT_SIZE, fill, shadow);
    }

    Render_StopCamera();
}

static void OptionsMenu_Destroy([[maybe_unused]] RayScene* scene) {
    UnloadTexture(bg.image);
}

static RayScene scene;
Scene_MakeSceneCode(scene, OptionsMenu_SetScene, OptionsMenu_Create, OptionsMenu_Update, OptionsMenu_Draw, OptionsMenu_Destroy)
