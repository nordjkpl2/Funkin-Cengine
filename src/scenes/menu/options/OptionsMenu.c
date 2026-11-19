/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenu.c - menu de opcoes (cena). Os valores salvos ficam em GameData.
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/MenuState.h"
#include "scenes/menu/options/OptionsMenu.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"
#include "widgets-and-menus-ui/SearchBox.h"
#include "scenes/menu/MenuMusic.h"
#include <stdio.h>

#define SCREEN_H 720.0f

// texto e header ta tudo aqui
#define SCROLL_LERP_SPEED 10.0f
#define ROW_FONT_SIZE 70
#define ROW_PADDING 30
#define HEADER_FONT_SIZE 90
#define HEADER_PADDING 10
#define TEXT_SHADOW 6
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

// Quadro (moldura) ao lado de "[ Visual Options ]".
#define FRAME_WIDTH 50.0f
#define FRAME_HEIGHT 38.0f
#define FRAME_STROKE 6.0f

// Icone de TV ao lado das opcoes de Window (Framerate ate V-SYNC); o texto dessas linhas recua para caber.
#define TV_X 46.0f
#define TV_W 50.0f
#define TV_H 34.0f
#define TV_STROKE 5.0f
#define WINDOW_ROW_SHIFT 14.0f

// Coracao com adaga cravada ao lado de "Health Drain" (esquerda da linha).
#define HEALTH_ICON_CX 58.0f
#define HEALTH_HEART_R 11.0f

// Uma seta a esquerda de Scheme, Scroll Style e Ghost Tapping.
#define KEY_ARROW_CX 58.0f
#define KEY_ARROW_HALF 11.0f

// Icones extras (todos na mesma coluna esquerda das linhas).
#define EXTRA_ICON_CX 58.0f
#define STRETCH_HALF 20.0f     // meia largura da seta dupla <-->
#define STRETCH_HEAD 9.0f      // tamanho da ponta da seta
#define TEXT_ICON_HALF 14.0f   // meia altura da caixa com o "T"
#define BOP_ICON_R 7.0f        // raio de cada bolinha do Bopping
#define KEYMON_ICON_HALF 11.0f // meia largura do quadrado do Keys Monitor

// Nota musical ao lado de "[Musica]".
#define NOTE_HEAD_RADIUS 13.0f
#define NOTE_STEM_W 7.0f
#define NOTE_STEM_H 56.0f
#define NOTE_FLAG_W 16.0f
#define NOTE_FLAG_H 22.0f

// Aba Musica: Auto passa para a proxima faixa quando a atual termina (so com o menu aberto).
#define MUSIC_SLOTS 65 // 64 faixas + Auto

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
    OPTION_THREADS,
    OPTION_HEADER_GAMEPLAY,
    OPTION_KEYBOARD_SCHEME,
    OPTION_SCROLL_STYLE,
    OPTION_GHOST,
    OPTION_HITSOUND,
    OPTION_HEALTH_DRAIN_PLAY,
    OPTION_HEADER_VISUAL,
    OPTION_SHOW_SCORE,
    OPTION_SCORE_POSITION,
    OPTION_BOPPING_ICONS,
    OPTION_STRUM_BG,
    OPTION_STRUM_MOBILE,
    OPTION_ZOOMFACTOR_UI,
    OPTION_ZOOMFACTOR_GAME,
    OPTION_HEADER_OTHER,
    OPTION_FILL_SCREEN,
    OPTION_BOTPLAY,
    OPTION_HEALTH_DRAIN_BOTPLAY,
    OPTION_HEADER_MUSIC,
    OPTION_MENU_MUSIC,
    OPTION_COUNT
} OptionOrder;

static const char* ghost_names[3] = {"On", "50/50", "Off"};
static const char* scroll_names[3] = {"Normal", "Downscroll", "Middlescroll"};
static const char* thread_names[2] = {"Auto", "Limited"};
static const char* score_position_names[2] = {"Cengine", "Psych"};
static const char* strum_bg_names[11] = {"Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"};

#define THREADS_MAX JOBS_MAX_THREADS
static int limitedThreads = 2; // ultimo numero usado no modo Limited (volta ao alternar)

/* Threads do modo Auto: nucleos - 1 (minimo 1), igual ao Jobs. */
static int AutoThreads(void) {
    int n = Thread_CoreCount() - 1;
    if (n < 1) n = 1;
    return n > THREADS_MAX ? THREADS_MAX : n;
}

static Option options[OPTION_COUNT] = {
    [OPTION_HEADER_WINDOW] = {.text = "[Window]", .type = OPTION_VALUE_HEADER},
    [OPTION_FPS] = {.text = "Framerate", .type = OPTION_VALUE_INT, .min = 60, .max = 2000, .step = 10, .altStep = 100},
    [OPTION_UNCAPPED_FPS] = {.text = "Uncapped FPS", .type = OPTION_VALUE_BOOL},
    [OPTION_SHOW_FPS] = {.text = "Show FPS", .type = OPTION_VALUE_BOOL},
    [OPTION_VSYNC] = {.text = "V-SYNC", .type = OPTION_VALUE_BOOL},
    [OPTION_THREADS] = {.text = "Threads", .type = OPTION_VALUE_LIST, .options = thread_names, .step = 1, .max = 1, .min = 0},
    [OPTION_HEADER_GAMEPLAY] = {.text = "[Gameplay]", .type = OPTION_VALUE_HEADER},
    [OPTION_KEYBOARD_SCHEME] = {.text = "Scheme", .type = OPTION_VALUE_LIST, .options = game_available_schemes, .step = 1, .max = SCHEME_COUNT - 1, .min = 0},
    [OPTION_SCROLL_STYLE] = {.text = "Scroll Style", .type = OPTION_VALUE_LIST, .options = scroll_names, .step = 1, .max = 2, .min = 0},
    [OPTION_GHOST] = {.text = "Ghost Tapping", .type = OPTION_VALUE_LIST, .options = ghost_names, .step = 1, .max = 2, .min = 0},
    [OPTION_HITSOUND] = {.text = "Hit Sound", .type = OPTION_VALUE_BOOL},
    [OPTION_HEALTH_DRAIN_PLAY] = {.text = "Health Drain (Play)", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_VISUAL] = {.text = "[Visual]", .type = OPTION_VALUE_HEADER},
    [OPTION_SHOW_SCORE] = {.text = "Show Score", .type = OPTION_VALUE_BOOL},
    [OPTION_SCORE_POSITION] = {.text = "Score Position", .type = OPTION_VALUE_LIST, .options = score_position_names, .step = 1, .max = 1, .min = 0},
    [OPTION_BOPPING_ICONS] = {.text = "Bopping Icons", .type = OPTION_VALUE_BOOL},
    [OPTION_STRUM_BG] = {.text = "Strumline Background", .type = OPTION_VALUE_LIST, .options = strum_bg_names, .step = 1, .min = 0, .max = 10},
    [OPTION_STRUM_MOBILE] = {.text = "Keys Monitor", .type = OPTION_VALUE_BOOL},
    [OPTION_ZOOMFACTOR_UI] = {.text = "UI beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_ZOOMFACTOR_GAME] = {.text = "Game beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_HEADER_OTHER] = {.text = "[Other]", .type = OPTION_VALUE_HEADER},
    [OPTION_FILL_SCREEN] = {.text = "Stretch", .type = OPTION_VALUE_BOOL},
    [OPTION_BOTPLAY] = {.text = "Botplay", .type = OPTION_VALUE_BOOL},
    [OPTION_HEALTH_DRAIN_BOTPLAY] = {.text = "Health Drain (Botplay)", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_MUSIC] = {.text = "[Musica]", .type = OPTION_VALUE_HEADER},
    [OPTION_MENU_MUSIC] = {.text = "Track", .type = OPTION_VALUE_LIST, .step = 1, .min = 0}, // options/max: Create
};

static float scrollOffset; // posicao suavizada da lista (em linhas)
static float windowLogoX;  // calculado uma vez no Create
static float otherLogoX;
static float gameplayLogoX;
static float visualLogoX;
static int currentOption = OPTION_FPS; // comeca na primeira opcao, nao no cabecalho
static Camera2D cam;
static RayGraphicObject bg;
static float musicLogoX;
static const char* musicNames[MUSIC_SLOTS]; // rotulos das faixas (+ Auto no fim)
static char autoLabel[160];                 // "Auto (faixa atual)"
static int autoTrack;                       // faixa que o Auto esta tocando

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
        case OPTION_VALUE_LIST:
            if (o == options + OPTION_THREADS)   // mostra quantas threads estao valendo
                (void)snprintf(out, size, "%s: << %s (%d) >>", o->text, o->options[(int)o->value],
                               gameOptions.threadLimit > 0 ? gameOptions.threadLimit : AutoThreads());
            else
                (void)snprintf(out, size, "%s: << %s >>", o->text, o->options[(int)o->value]);
            break;
        default: break;
    }

    o->dirty = false;
}

/* ------------------------------------------------------------------ */
/* Aplicar mudancas de valor                                           */
/* ------------------------------------------------------------------ */

/* ---- Aba Musica ---- */

static bool MusicAutoOn(void) {
    return (int)options[OPTION_MENU_MUSIC].value >= MenuMusic_Count();
}

/* Rotulo do Auto mostra a faixa que esta tocando. */
static void RefreshAutoLabel(void) {
    if (MenuMusic_Count() > 0)
        (void)snprintf(autoLabel, sizeof(autoLabel), "Auto (%s)", MenuMusic_Label(autoTrack));
    else
        (void)snprintf(autoLabel, sizeof(autoLabel), "Auto");
    options[OPTION_MENU_MUSIC].dirty = true;
}

/* Escolha mudou: toca a faixa (ou entra no Auto) e guarda a faixa escolhida. */
static void ApplyMusicValue(void) {
    const int v = (int)options[OPTION_MENU_MUSIC].value;

    if (MusicAutoOn()) {
        autoTrack = MenuMusic_SavedIndex();
        MenuMusic_Play(autoTrack, 0);
        RefreshAutoLabel();
        return;
    }

    MenuMusic_Play(v, 1);
    (void)snprintf(gameOptions.menuMusic, sizeof(gameOptions.menuMusic), "%s", MenuMusic_FileName(v));
}

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
        case OPTION_THREADS:
            gameOptions.threadLimit = (int)o->value == 1 ? limitedThreads : 0;
            Jobs_SetThreads(gameOptions.threadLimit);
            break;
        case OPTION_KEYBOARD_SCHEME:   gameOptions.scheme = (int)o->value; break;
        case OPTION_SCROLL_STYLE:     gameOptions.scroll = (char)(int)o->value; break;
        case OPTION_GHOST:             gameOptions.ghost = (char)(int)o->value; break;
        case OPTION_ZOOMFACTOR_GAME:   gameOptions.zoomFactorGAME = o->value; break;
        case OPTION_ZOOMFACTOR_UI:     gameOptions.zoomFactorUI = o->value; break;
        case OPTION_SCORE_POSITION:    gameOptions.scorePosition = (char)(int)o->value; break;
        case OPTION_STRUM_BG:          gameOptions.strumBgPercent = (char)((int)o->value * 10); break;
        case OPTION_MENU_MUSIC:        ApplyMusicValue(); break;
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
        case OPTION_SHOW_FPS:      gameOptions.showFps = on; break;
        case OPTION_SHOW_SCORE:
            gameOptions.showScore = on;
            options[OPTION_SCORE_POSITION].active = on;   // so faz sentido escolher posicao com score visivel
            options[OPTION_SCORE_POSITION].dirty = true;
            break;
        case OPTION_BOPPING_ICONS: gameOptions.bopIcons = on; break;
        case OPTION_STRUM_MOBILE:  gameOptions.strumMobileVisible = on; break;
        case OPTION_FILL_SCREEN:   gameOptions.fillScreen = on; break;
        case OPTION_BOTPLAY:       gameOptions.botplay = on; break;
        case OPTION_HITSOUND:      gameOptions.hitSound = on; break;
        case OPTION_HEALTH_DRAIN_PLAY:    gameOptions.healthDrainPlay = on; break;
        case OPTION_HEALTH_DRAIN_BOTPLAY: gameOptions.healthDrainBotplay = on; break;
    }

    Option_RebuildText(o);
}

/* ------------------------------------------------------------------ */
/* Navegacao                                                           */
/* ------------------------------------------------------------------ */

/* Lupa (so o icone; o campo abre ao clicar). Filtra as opcoes pelo nome. */
#define SEARCH_X 940.0f
#define SEARCH_Y 20.0f
#define SEARCH_W 320.0f
#define SEARCH_H 44.0f
static SearchBox search;

/* Secao tem alguma opcao que bate com o filtro? */
static bool Section_HasMatch(int header) {
    for (int i = header + 1; i < OPTION_COUNT && options[i].type != OPTION_VALUE_HEADER; i++)
        if (SearchBox_Match(&search, options[i].text)) return true;
    return false;
}

/* Com filtro, aparecem so as opcoes (e secoes) que batem, mesmo se recolhidas.
   Sem filtro: cabecalhos sempre; opcoes somem quando a secao esta recolhida. */
static bool Option_ComputeVisible(int i) {
    const bool filtering = SearchBox_HasFilter(&search);
    if (options[i].type == OPTION_VALUE_HEADER)
        return !filtering || Section_HasMatch(i);
    if (filtering)
        return SearchBox_Match(&search, options[i].text);
    return !options[options[i].header].collapsed;
}

/* Cache do layout: so recalcula quando o filtro muda ou uma secao abre/fecha (layoutDirty). */
static bool layoutDirty = true;
static bool visibleCache[OPTION_COUNT];
static int visualRowCache[OPTION_COUNT]; // linhas visiveis antes de cada opcao

static void Layout_Refresh(void) {
    if (!layoutDirty) return;

    int row = 0;
    for (int i = 0; i < OPTION_COUNT; i++) {
        visibleCache[i] = Option_ComputeVisible(i);
        visualRowCache[i] = row;
        if (visibleCache[i]) row++;
    }
    layoutDirty = false;
}

static bool Option_IsVisible(int i) {
    Layout_Refresh();
    return visibleCache[i];
}

/* Cabecalhos sao selecionaveis (para abrir/recolher); opcoes, so se visiveis e ativas. */
static bool Option_IsSelectable(int i) {
    if (options[i].type == OPTION_VALUE_HEADER) return true;
    return Option_IsVisible(i) && options[i].active;
}

/* Posicao da opcao na tela, em linhas (ignora as recolhidas). */
static int Option_VisualIndex(int index) {
    Layout_Refresh();
    return visualRowCache[index];
}

/* Anda para a proxima linha selecionavel. */
static void MoveSelection(int dir) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        currentOption = (currentOption + dir + OPTION_COUNT) % OPTION_COUNT;
        if (Option_IsSelectable(currentOption)) break;
    }
}

/* ------------------------------------------------------------------ */
/* Teclas customizadas (esquema "Custom")                              */
/* ------------------------------------------------------------------ */

static const char* const LANE_NAMES[4] = {"LEFT", "DOWN", "UP", "RIGHT"};

static bool capturing;           // tela preta de captura aberta
static int captureStep;          // 0..3 = esperando a tecla dessa direcao; 4 = confirmar
static int captureKeys[4];
static const char* captureMessage = "";

/* Nome curto da tecla para mostrar na tela. */
static void KeyName(int key, char* out, size_t size) {
    static const struct { int key; const char* name; } named[] = {
        {KEY_SPACE, "SPACE"}, {KEY_LEFT, "LEFT"}, {KEY_RIGHT, "RIGHT"}, {KEY_UP, "UP"}, {KEY_DOWN, "DOWN"},
        {KEY_COMMA, ","}, {KEY_PERIOD, "."}, {KEY_SEMICOLON, ";"}, {KEY_APOSTROPHE, "'"}, {KEY_SLASH, "/"},
        {KEY_BACKSLASH, "\\"}, {KEY_LEFT_BRACKET, "["}, {KEY_RIGHT_BRACKET, "]"}, {KEY_MINUS, "-"},
        {KEY_EQUAL, "="}, {KEY_GRAVE, "`"}, {KEY_TAB, "TAB"}, {KEY_LEFT_SHIFT, "L-SHIFT"},
        {KEY_RIGHT_SHIFT, "R-SHIFT"}, {KEY_LEFT_CONTROL, "L-CTRL"}, {KEY_RIGHT_CONTROL, "R-CTRL"},
        {KEY_LEFT_ALT, "L-ALT"}, {KEY_RIGHT_ALT, "R-ALT"}, {KEY_INSERT, "INSERT"}, {KEY_DELETE, "DELETE"},
        {KEY_HOME, "HOME"}, {KEY_END, "END"}, {KEY_PAGE_UP, "PGUP"}, {KEY_PAGE_DOWN, "PGDN"},
    };

    if (key >= KEY_A && key <= KEY_Z)         (void)snprintf(out, size, "%c", 'A' + (key - KEY_A));
    else if (key >= KEY_ZERO && key <= KEY_NINE) (void)snprintf(out, size, "%c", '0' + (key - KEY_ZERO));
    else if (key >= KEY_KP_0 && key <= KEY_KP_9) (void)snprintf(out, size, "NUM %d", key - KEY_KP_0);
    else if (key >= KEY_F1 && key <= KEY_F12)    (void)snprintf(out, size, "F%d", key - KEY_F1 + 1);
    else {
        for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++)
            if (named[i].key == key) { (void)snprintf(out, size, "%s", named[i].name); return; }
        (void)snprintf(out, size, "KEY %d", key);
    }
}

static void Capture_Begin(void) {
    capturing = true;
    captureStep = 0;
    captureMessage = "";
    for (int k = 0; k < 4; k++) captureKeys[k] = 0;
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
}

static void Capture_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE)) {   // cancela e mantem as teclas antigas
        capturing = false;
        return;
    }

    if (captureStep == 4) {
        if (IsKeyPressed(KEY_ENTER)) {
            for (int k = 0; k < 4; k++) game_scheme_keys[SCHEME_CUSTOM][k] = captureKeys[k];
            gameOptions.scheme = SCHEME_CUSTOM;
            options[OPTION_KEYBOARD_SCHEME].value = SCHEME_CUSTOM;
            options[OPTION_KEYBOARD_SCHEME].dirty = true;
            GameData_Flush();
            capturing = false;
        }
        else if (IsKeyPressed(KEY_BACKSPACE)) {
            captureStep = 3;
            captureMessage = "";
        }
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {   // volta uma direcao
        if (captureStep > 0) captureStep--;
        captureKeys[captureStep] = 0;
        captureMessage = "";
        return;
    }

    const int key = GetKeyPressed();
    if (key == 0) return;
    if (key == KEY_ENTER || key == KEY_KP_ENTER) { captureMessage = "ENTER e reservado"; return; }

    for (int k = 0; k < captureStep; k++) {
        if (captureKeys[k] == key) { captureMessage = "Tecla ja usada"; return; }
    }

    captureKeys[captureStep++] = key;
    captureMessage = "";
}

static void Capture_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, BLACK);
    DrawTextEx(mainFont, "CUSTOM KEYS", (Vector2) {80, 50}, 70, TEXT_SPACING, YELLOW);

    for (int k = 0; k < 4; k++) {
        char name[24] = "...";
        if (captureKeys[k] != 0 && k < captureStep) KeyName(captureKeys[k], name, sizeof(name));

        char line[64];
        (void)snprintf(line, sizeof(line), "%-6s  %s", LANE_NAMES[k], name);
        Color color = k < captureStep ? GREEN : (k == captureStep ? WHITE : GRAY);
        DrawTextEx(mainFont, line, (Vector2) {120, 190.0f + (float)k * 80.0f}, 56, TEXT_SPACING, color);
    }

    if (captureStep < 4) {
        char prompt[48];
        (void)snprintf(prompt, sizeof(prompt), "Press the key for %s", LANE_NAMES[captureStep]);
        DrawTextEx(mainFont, prompt, (Vector2) {80, 540}, 36, 3, WHITE);
        DrawTextEx(mainFont, "BACKSPACE back     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    } else {
        DrawTextEx(mainFont, "ENTER to confirm", (Vector2) {80, 540}, 36, 3, GREEN);
        DrawTextEx(mainFont, "BACKSPACE back     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
    }
    if (captureMessage[0] != 0)
        DrawTextEx(mainFont, captureMessage, (Vector2) {80, 595}, 28, 2, RED);
}

/* ------------------------------------------------------------------ */
/* Tela do numero de threads (modo "Limited")                          */
/* ------------------------------------------------------------------ */

static bool threadsScreen;   // tela preta aberta
static int threadsInput;     // numero sendo digitado (0 = vazio)

static int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void ThreadsScreen_Begin(void) {
    threadsScreen = true;
    threadsInput = limitedThreads;
    while (GetKeyPressed() != 0) {}   // descarta o Enter que abriu a tela
}

static void ThreadsScreen_Update(void) {
    if (IsKeyPressed(KEY_ESCAPE)) {   // cancela e mantem o numero antigo
        threadsScreen = false;
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        const int n = ClampInt(threadsInput, 1, THREADS_MAX);
        limitedThreads = n;
        gameOptions.threadLimit = n;
        Jobs_SetThreads(n);
        options[OPTION_THREADS].dirty = true;
        GameData_Flush();
        threadsScreen = false;
        return;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_RIGHT)) threadsInput = ClampInt(threadsInput + 1, 1, THREADS_MAX);
    else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT)) threadsInput = ClampInt(threadsInput - 1, 1, THREADS_MAX);
    else if (IsKeyPressed(KEY_BACKSPACE)) threadsInput /= 10;

    for (int d = 0; d <= 9; d++) {
        if (IsKeyPressed(KEY_ZERO + d) || IsKeyPressed(KEY_KP_0 + d)) {
            threadsInput = threadsInput * 10 + d;
            if (threadsInput > THREADS_MAX) threadsInput = d;   // 3o digito recomeca
        }
    }
}

static void ThreadsScreen_Draw(void) {
    DrawRectangle(0, 0, 1280, 720, BLACK);
    DrawTextEx(mainFont, "THREADS", (Vector2) {80, 50}, 70, TEXT_SPACING, YELLOW);

    char line[64];
    (void)snprintf(line, sizeof(line), "CPU threads: %d     Auto: %d", Thread_CoreCount(), AutoThreads());
    DrawTextEx(mainFont, line, (Vector2) {120, 190}, 40, 3, GRAY);

    if (threadsInput > 0) (void)snprintf(line, sizeof(line), "Limit: %d", threadsInput);
    else (void)snprintf(line, sizeof(line), "Limit: _");
    DrawTextEx(mainFont, line, (Vector2) {120, 290}, 80, TEXT_SPACING, WHITE);

    DrawTextEx(mainFont, "Type a number (1-64)  or  UP / DOWN", (Vector2) {80, 480}, 32, 3, WHITE);
    DrawTextEx(mainFont, "ENTER confirm", (Vector2) {80, 540}, 36, 3, threadsInput > 0 ? GREEN : GRAY);
    DrawTextEx(mainFont, "BACKSPACE erase     ESC cancel", (Vector2) {80, 650}, 24, 2, GRAY);
}

/* ------------------------------------------------------------------ */
/* Cena                                                                */
/* ------------------------------------------------------------------ */

/* X do logo ao lado de um cabecalho: logo depois do fim do texto (medido uma vez no Create). */
static float LogoXAfterHeader(int header) {
    const Vector2 size = MeasureTextEx(mainFont, options[header].text, HEADER_FONT_SIZE, TEXT_SPACING);
    return HEADER_X - TEXT_SHADOW + size.x + LOGO_MARGIN;
}

static void OptionsMenu_Create([[maybe_unused]] RayScene* scene) {
    Render_DefaultRGT(&bg);
    bg.objType = RGT_IMAGE;
    bg.image = Render_LoadTexture("assets/images/freeplay/menuBGBlue.png");
    Render_DefaultCamera(&cam);

    windowLogoX = LogoXAfterHeader(OPTION_HEADER_WINDOW);
    otherLogoX = LogoXAfterHeader(OPTION_HEADER_OTHER);
    gameplayLogoX = LogoXAfterHeader(OPTION_HEADER_GAMEPLAY);
    visualLogoX = LogoXAfterHeader(OPTION_HEADER_VISUAL);
    musicLogoX = LogoXAfterHeader(OPTION_HEADER_MUSIC);

    SearchBox_Init(&search, (Rectangle) {SEARCH_X, SEARCH_Y, SEARCH_W, SEARCH_H});
    SearchBox_SetCompact(&search, true);

    // Cada opcao guarda o cabecalho da sua secao; todas as secoes comecam abertas.
    layoutDirty = true;
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
    options[OPTION_SHOW_FPS].value = gameOptions.showFps;
    options[OPTION_SHOW_SCORE].value = gameOptions.showScore;
    options[OPTION_SCORE_POSITION].value = gameOptions.scorePosition;
    options[OPTION_SCORE_POSITION].active = gameOptions.showScore;
    options[OPTION_BOPPING_ICONS].value = gameOptions.bopIcons;
    options[OPTION_STRUM_BG].value = gameOptions.strumBgPercent / 10;
    options[OPTION_STRUM_MOBILE].value = gameOptions.strumMobileVisible;
    options[OPTION_VSYNC].value = gameOptions.vsync;
    if (gameOptions.threadLimit > 0) limitedThreads = gameOptions.threadLimit;
    options[OPTION_THREADS].value = gameOptions.threadLimit > 0;
    options[OPTION_KEYBOARD_SCHEME].value = gameOptions.scheme;
    options[OPTION_SCROLL_STYLE].value = gameOptions.scroll;
    options[OPTION_GHOST].value = gameOptions.ghost;
    options[OPTION_HITSOUND].value = gameOptions.hitSound;
    options[OPTION_HEALTH_DRAIN_PLAY].value = gameOptions.healthDrainPlay;
    options[OPTION_HEALTH_DRAIN_BOTPLAY].value = gameOptions.healthDrainBotplay;
    options[OPTION_ZOOMFACTOR_GAME].value = gameOptions.zoomFactorGAME;
    options[OPTION_ZOOMFACTOR_UI].value = gameOptions.zoomFactorUI;
    options[OPTION_FILL_SCREEN].value = gameOptions.fillScreen;
    options[OPTION_BOTPLAY].value = gameOptions.botplay;

    // Faixas de assets/music/menu, com Auto no fim. A aba comeca na faixa salva.
    const int trackCount = MenuMusic_Count();
    for (int t = 0; t < trackCount; t++) musicNames[t] = MenuMusic_Label(t);
    options[OPTION_MENU_MUSIC].options = musicNames;
    options[OPTION_MENU_MUSIC].max = (float)trackCount;
    options[OPTION_MENU_MUSIC].value = (float)MenuMusic_SavedIndex();
    autoTrack = MenuMusic_SavedIndex();
    RefreshAutoLabel();
    musicNames[trackCount] = autoLabel;

    for (int i = 0; i < OPTION_COUNT; i++)
        Option_RebuildText(options + i);

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

/* Quadro (moldura com borda grossa), com a mesma sombra do texto. */
static void DrawVisualLogo(float rowY, Color fill, Color shadow) {
    const float cy = rowY + LOGO_TOP + LOGO_SQUARE + LOGO_GAP * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        DrawRectangleLinesEx((Rectangle) {visualLogoX + shift, cy - FRAME_HEIGHT * 0.5f + shift, FRAME_WIDTH, FRAME_HEIGHT},
                             FRAME_STROKE, color);
    }
}

/* Nota musical (colcheia), com a mesma sombra do texto. */
static void DrawMusicLogo(float rowY, Color fill, Color shadow) {
    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float top = rowY + LOGO_TOP + shift;
        const float headX = musicLogoX + NOTE_HEAD_RADIUS + shift;
        const float headY = top + NOTE_STEM_H;
        const float stemLeft = headX + NOTE_HEAD_RADIUS - NOTE_STEM_W;
        const float stemX = stemLeft + NOTE_STEM_W * 0.5f;

        DrawCircleV((Vector2) {headX, headY}, NOTE_HEAD_RADIUS, color);
        DrawRectangleRec((Rectangle) {stemLeft, top, NOTE_STEM_W, NOTE_STEM_H}, color);
        DrawRoundLine((Vector2) {stemX, top}, (Vector2) {stemX + NOTE_FLAG_W, top + NOTE_FLAG_H}, color);
    }
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

/* TV (retangulo com antenas), com a mesma sombra do texto. */
static void DrawTvIcon(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float left = TV_X + shift;
        const float top = cy - TV_H * 0.5f + shift;
        const float cx = left + TV_W * 0.5f;

        DrawLineEx((Vector2) {cx - 6.0f, top}, (Vector2) {cx - 14.0f, top - 12.0f}, TV_STROKE * 0.6f, color);
        DrawLineEx((Vector2) {cx + 6.0f, top}, (Vector2) {cx + 14.0f, top - 12.0f}, TV_STROKE * 0.6f, color);
        DrawRectangleLinesEx((Rectangle) {left, top, TV_W, TV_H}, TV_STROKE, color);
    }
}

/* Triangulo preenchido, independente da ordem dos vertices (DrawTriangle exige um sentido). */
static void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    const float cross = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (cross > 0.0f) {   // DrawTriangle do raylib so desenha no sentido oposto ao que ficava invisivel
        const Vector2 t = b;
        b = c;
        c = t;
    }
    DrawTriangle(a, b, c, color);
}

/* Coracao: curva parametrica classica, preenchida em leque a partir do centro. r = meia largura. */
static void DrawHeart(float cx, float cy, float r, Color color) {
    enum { HEART_SEGMENTS = 32 };
    static Vector2 unit[HEART_SEGMENTS]; // contorno normalizado (largura ~32), calculado uma vez
    static bool unitReady;

    if (!unitReady) {
        for (int i = 0; i < HEART_SEGMENTS; i++) {
            const float t = 2.0f * PI * (float)i / (float)HEART_SEGMENTS;
            const float s = sinf(t);
            const float x = 16.0f * s * s * s;
            const float y = 13.0f * cosf(t) - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t);
            unit[i] = (Vector2) {x, -(y + 2.5f)}; // +2.5 centraliza verticalmente
        }
        unitReady = true;
    }

    const float scale = r / 9.0f;
    Vector2 pts[HEART_SEGMENTS];
    for (int i = 0; i < HEART_SEGMENTS; i++)
        pts[i] = (Vector2) {cx + unit[i].x * scale, cy + unit[i].y * scale};

    for (int i = 0; i < HEART_SEGMENTS; i++)
        FillTriangle((Vector2) {cx, cy}, pts[i], pts[(i + 1) % HEART_SEGMENTS], color);
}

/* Adaga na vertical: ponta para baixo, cruzando o coracao (cx, cy = centro da lamina). */
static void DrawDagger(float cx, float cy, Color color) {
    FillTriangle((Vector2) {cx - 5.0f, cy - 8.0f}, (Vector2) {cx + 5.0f, cy - 8.0f}, (Vector2) {cx, cy + 12.0f}, color); // lamina
    DrawRectangleRec((Rectangle) {cx - 12.0f, cy - 13.0f, 24.0f, 5.0f}, color);  // guarda
    DrawRectangleRec((Rectangle) {cx - 3.0f, cy - 19.0f, 6.0f, 7.0f}, color);    // cabo
    DrawCircleV((Vector2) {cx, cy - 22.0f}, 4.0f, color);                       // pomo
}

/* Coracao com adaga cravada, com a mesma sombra do texto. */
static void DrawHealthIcons(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = HEALTH_ICON_CX + shift;

        DrawHeart(cx, cy + 3.0f + shift, HEALTH_HEART_R, color);
        DrawDagger(cx, cy + shift, color);
    }
}

/* Uma seta (triangulo apontando para a direita), com a mesma sombra do texto. */
static void DrawKeyArrow(float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;
    const float s = KEY_ARROW_HALF;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = KEY_ARROW_CX + shift;
        const float y = cy + shift;

        FillTriangle((Vector2) {cx - s * 0.5f, y - s}, (Vector2) {cx - s * 0.5f, y + s}, (Vector2) {cx + s * 0.8f, y}, color);
    }
}

/* Seta dupla <-->, para Stretch. */
static void DrawStretchIcon(float cx, float cy, Color color) {
    const float h = STRETCH_HALF;
    const float head = STRETCH_HEAD;

    DrawRoundLine((Vector2) {cx - h, cy}, (Vector2) {cx + h, cy}, color);
    FillTriangle((Vector2) {cx - h - 4.0f, cy}, (Vector2) {cx - h + head, cy - head * 0.7f}, (Vector2) {cx - h + head, cy + head * 0.7f}, color);
    FillTriangle((Vector2) {cx + h + 4.0f, cy}, (Vector2) {cx + h - head, cy - head * 0.7f}, (Vector2) {cx + h - head, cy + head * 0.7f}, color);
}

/* Caixa com um "T" dentro, para as opcoes de texto (Show Score / Score Position). */
static void DrawTextIcon(float cx, float cy, Color color) {
    const float h = TEXT_ICON_HALF;
    DrawRectangleLinesEx((Rectangle) {cx - h, cy - h, h * 2.0f, h * 2.0f}, 4.0f, color);
    DrawTextEx(mainFont, "T", (Vector2) {cx - 8.0f, cy - 17.0f}, 34, 0.0f, color);
}

/* Duas bolinhas, uma um pouco mais alta que a outra (Bopping Icons). */
static void DrawBopIcon(float cx, float cy, Color color) {
    DrawCircleV((Vector2) {cx - 9.0f, cy + 3.0f}, BOP_ICON_R, color);
    DrawCircleV((Vector2) {cx + 9.0f, cy - 3.0f}, BOP_ICON_R, color);
}

/* Quadrado, para Keys Monitor. */
static void DrawSquareIcon(float cx, float cy, Color color) {
    const float h = KEYMON_ICON_HALF;
    DrawRectangleLinesEx((Rectangle) {cx - h, cy - h, h * 2.0f, h * 2.0f}, 5.0f, color);
}

/* Icone da linha (so para as opcoes que tem), com a mesma sombra do texto. */
static void DrawExtraIcon(int i, float rowY, Color fill, Color shadow) {
    const float cy = rowY + ROW_FONT_SIZE * 0.5f;

    for (int pass = 0; pass < 2; pass++) {
        const Color color = pass == 0 ? shadow : fill;
        const float shift = pass == 0 ? TEXT_SHADOW : 0.0f;
        const float cx = EXTRA_ICON_CX + shift;
        const float y = cy + shift;

        switch (i) {
            case OPTION_FILL_SCREEN:    DrawStretchIcon(cx, y, color); break;
            case OPTION_SHOW_SCORE:
            case OPTION_SCORE_POSITION: DrawTextIcon(cx, y, color); break;
            case OPTION_BOPPING_ICONS:  DrawBopIcon(cx, y, color); break;
            case OPTION_STRUM_MOBILE:   DrawSquareIcon(cx, y, color); break;
            default: return;
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

    if (currentOption == OPTION_THREADS && (int)options[OPTION_THREADS].value == 1)
        DrawTextEx(mainFont, "ENTER: set thread count", (Vector2) {ROW_X, SCREEN_H - 50.0f}, 24, 2, (Color) {245, 220, 120, 255});

    SearchBox_Draw(&search, mainFont);

    if (capturing)
        Capture_Draw();
    else if (threadsScreen)
        ThreadsScreen_Draw();

    Render_StopCamera();
}

static void OptionsMenu_Destroy([[maybe_unused]] RayScene* scene) {
    UnloadTexture(bg.image);
}

static RayScene scene;
Scene_MakeSceneCode(scene, OptionsMenu_SetScene, OptionsMenu_Create, OptionsMenu_Update, OptionsMenu_Draw, OptionsMenu_Destroy)
