/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuValues.c - tabela das opcoes, aplicar valores no jogo, aba Musica,
 * visibilidade (filtro da lupa e secoes recolhidas) e navegacao.
 */
#include "scenes/menu/options/OptionsMenuInternal.h"
#include "scenes/menu/MenuMusic.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "threads/Thread.h"
#include "threads/cpu/Jobs.h"
#include <stdio.h>

static const char* ghost_names[3] = {"On", "50/50", "Off"};
static const char* scroll_names[3] = {"Normal", "Downscroll", "Middlescroll"};
static const char* thread_names[2] = {"Auto", "Limited"};
static const char* score_position_names[2] = {"Cengine", "Psych"};
static const char* strum_bg_names[11] = {"Off", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"};

int limitedThreads = 2; // ultimo numero usado no modo Limited (volta ao alternar)

/* Threads do modo Auto: nucleos - 1 (minimo 1), igual ao Jobs. */
int AutoThreads(void) {
    int n = Thread_CoreCount() - 1;
    if (n < 1) n = 1;
    return n > THREADS_MAX ? THREADS_MAX : n;
}

Option options[OPTION_COUNT] = {
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

int currentOption = OPTION_FPS; // comeca na primeira opcao, nao no cabecalho
int autoTrack;                  // faixa que o Auto esta tocando
static const char* musicNames[MUSIC_SLOTS]; // rotulos das faixas (+ Auto no fim)
static char autoLabel[160];                 // "Auto (faixa atual)"

void Option_RebuildText(Option* o) {
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

bool MusicAutoOn(void) {
    return (int)options[OPTION_MENU_MUSIC].value >= MenuMusic_Count();
}

/* Rotulo do Auto mostra a faixa que esta tocando. */
void RefreshAutoLabel(void) {
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
void ApplyNumValue(void) {
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
void ApplyBoolValue(void) {
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
SearchBox search;

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
bool layoutDirty = true;
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

bool Option_IsVisible(int i) {
    Layout_Refresh();
    return visibleCache[i];
}

/* Cabecalhos sao selecionaveis (para abrir/recolher); opcoes, so se visiveis e ativas. */
bool Option_IsSelectable(int i) {
    if (options[i].type == OPTION_VALUE_HEADER) return true;
    return Option_IsVisible(i) && options[i].active;
}

/* Posicao da opcao na tela, em linhas (ignora as recolhidas). */
int Option_VisualIndex(int index) {
    Layout_Refresh();
    return visualRowCache[index];
}

/* Anda para a proxima linha selecionavel. */
void MoveSelection(int dir) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        currentOption = (currentOption + dir + OPTION_COUNT) % OPTION_COUNT;
        if (Option_IsSelectable(currentOption)) break;
    }
}

/* Carrega os valores atuais nas opcoes (chamado no Create da cena). */
void Options_LoadValues(void) {
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
}
