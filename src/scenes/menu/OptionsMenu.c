/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenu.c - menu de opcoes (cena). Os valores salvos ficam em GameData.
 */
#include "scenes/AllScenes.h"

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
static int currentOption;
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

static char Option_IsSelectable(const Option* o) {
    return o->type != OPTION_VALUE_HEADER && o->active;
}

/* Anda para a proxima opcao selecionavel (pula cabecalhos e inativas). */
static void MoveSelection(int dir) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        currentOption = (currentOption + dir + OPTION_COUNT) % OPTION_COUNT;
        if (Option_IsSelectable(options + currentOption)) break;
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

    for (int i = 0; i < OPTION_COUNT; i++) {
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

    if (!Option_IsSelectable(options + currentOption))
        MoveSelection(1);
}

static void OptionsMenu_Update([[maybe_unused]] RayScene* scene) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        GameData_Flush();
        MenuState_SetScene();
        return;
    }

    scrollOffset = Lerp(scrollOffset, (float)currentOption, RayGame_DeltaTime() * SCROLL_LERP_SPEED);

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) MoveSelection(1);
    else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) MoveSelection(-1);

    Option* cur = options + currentOption;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (cur->type == OPTION_VALUE_BOOL) {
            cur->value = !cur->value;
            ApplyBoolValue();
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

static void OptionsMenu_Draw([[maybe_unused]] RayScene* scene) {
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);

    float y = SCREEN_H / 2.0f - scrollOffset * ROW_HEIGHT;

    for (int i = 0; i < OPTION_COUNT; i++) {
        Option* o = options + i;
        const bool isHeader = o->type == OPTION_VALUE_HEADER;
        const float height = isHeader ? HEADER_HEIGHT : ROW_HEIGHT;
        const float drawY = y;
        y += height;

        if (drawY > CULL_BOTTOM)
            break; // as linhas descem em ordem: o resto tambem esta fora da tela
        if (drawY < CULL_TOP)
            continue;

        if (isHeader) {
            DrawOptionText(o->text, HEADER_X, drawY, HEADER_FONT_SIZE, YELLOW, BLACK);
            continue;
        }

        if (o->dirty) Option_RebuildText(o);

        const bool selected = currentOption == i;
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
