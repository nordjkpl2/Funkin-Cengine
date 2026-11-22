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
static const char* scroll_names[4] = {"Normal", "Downscroll", "Middlescroll", "Middledown"};
static const char* thread_names[2] = {"Auto", "Limited"};
static const char* score_position_names[2] = {"Cengine", "Psych"};
static const char* note_colors_names[1] = {"Edit"};
static const char* hidden_notes_names[4] = {"Off", "Alternate", "Only Playable", "Alternate Playable"};
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
    [OPTION_HEADER_RENDER] = {.text = "[Render]", .type = OPTION_VALUE_HEADER},
    [OPTION_STREAM_JSON] = {.text = "Streaming JSON (notes)", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_GAMEPLAY] = {.text = "[Gameplay]", .type = OPTION_VALUE_HEADER},
    [OPTION_KEYBOARD_SCHEME] = {.text = "Scheme", .type = OPTION_VALUE_LIST, .options = game_available_schemes, .step = 1, .max = SCHEME_COUNT - 1, .min = 0},
    [OPTION_SCROLL_STYLE] = {.text = "Scroll Style", .type = OPTION_VALUE_LIST, .options = scroll_names, .step = 1, .max = 3, .min = 0},
    [OPTION_GHOST] = {.text = "Ghost Tapping", .type = OPTION_VALUE_LIST, .options = ghost_names, .step = 1, .max = 2, .min = 0},
    [OPTION_HITSOUND] = {.text = "Hit Sound", .type = OPTION_VALUE_BOOL},
    [OPTION_HEALTH_DRAIN_PLAY] = {.text = "Health Drain (Play)", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_VISUAL] = {.text = "[Visual]", .type = OPTION_VALUE_HEADER},
    [OPTION_SCORE_LABEL] = {.text = "Score", .type = OPTION_VALUE_LABEL},
    [OPTION_SHOW_SCORE] = {.text = "Show Score", .type = OPTION_VALUE_BOOL},
    [OPTION_SCORE_POSITION] = {.text = "Score Position", .type = OPTION_VALUE_LIST, .options = score_position_names, .step = 1, .max = 1, .min = 0},
    [OPTION_ICONS_LABEL] = {.text = "Icons", .type = OPTION_VALUE_LABEL},
    [OPTION_BOPPING_ICONS] = {.text = "Bopping Icons", .type = OPTION_VALUE_BOOL},
    [OPTION_LOSE_ICON] = {.text = "Losing Icon", .type = OPTION_VALUE_BOOL},
    [OPTION_ZOOM_LABEL] = {.text = "Zoom", .type = OPTION_VALUE_LABEL},
    [OPTION_ZOOMFACTOR_UI] = {.text = "UI beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_ZOOMFACTOR_GAME] = {.text = "Game beat zoom", .type = OPTION_VALUE_FLOAT, .max = 3, .min = 0, .step = 0.1f, .altStep = 1},
    [OPTION_NOTES_LABEL] = {.text = "Notes", .type = OPTION_VALUE_LABEL},
    [OPTION_STRUM_BG] = {.text = "Strumline Background", .type = OPTION_VALUE_LIST, .options = strum_bg_names, .step = 1, .min = 0, .max = 10},
    [OPTION_STRUM_MOBILE] = {.text = "Keys Monitor", .type = OPTION_VALUE_BOOL},
    [OPTION_HIDDEN_NOTES] = {.text = "Hidden Notes", .type = OPTION_VALUE_LIST, .options = hidden_notes_names, .step = 1, .min = 0, .max = 3},
    [OPTION_NOTE_COLORS] = {.text = "Note Colors", .type = OPTION_VALUE_LIST, .options = note_colors_names, .step = 1, .min = 0, .max = 0},
    [OPTION_HEADER_OTHER] = {.text = "[Other]", .type = OPTION_VALUE_HEADER},
    [OPTION_FILL_SCREEN] = {.text = "Stretch", .type = OPTION_VALUE_BOOL},
    [OPTION_BOTPLAY] = {.text = "Botplay", .type = OPTION_VALUE_BOOL},
    [OPTION_HEALTH_DRAIN_BOTPLAY] = {.text = "Health Drain (Botplay)", .type = OPTION_VALUE_BOOL},
    [OPTION_HORIZONTAL_MENU] = {.text = "Horizontal Menu", .type = OPTION_VALUE_BOOL},
    [OPTION_HEADER_MUSIC] = {.text = "[Musica]", .type = OPTION_VALUE_HEADER},
    [OPTION_MENU_MUSIC] = {.text = "Track", .type = OPTION_VALUE_LIST, .step = 1, .min = 0}, // options/max: Create
    [OPTION_RESET_CONFIG] = {.text = "Reset Config to Default", .type = OPTION_VALUE_ACTION},
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
        case OPTION_VALUE_LABEL:
        case OPTION_VALUE_ACTION: (void)snprintf(out, size, "%s", o->text); break;
        case OPTION_VALUE_INT:   (void)snprintf(out, size, "%s: < %d >", o->text, (int)o->value); break;
        case OPTION_VALUE_FLOAT: (void)snprintf(out, size, "%s: < %.2f >", o->text, (double)o->value); break;
        case OPTION_VALUE_BOOL:  (void)snprintf(out, size, "%s: [%s]", o->text, o->value ? "ON" : "OFF"); break;
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
        case OPTION_HIDDEN_NOTES:      gameOptions.hiddenNotesAlt = (char)(int)o->value; break;
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
        case OPTION_LOSE_ICON:     gameOptions.loseIcon = on; break;
        case OPTION_STRUM_MOBILE:  gameOptions.strumMobileVisible = on; break;
        case OPTION_FILL_SCREEN:   gameOptions.fillScreen = on; break;
        case OPTION_STREAM_JSON:   gameOptions.streamingJsonParse = on; break;
        case OPTION_BOTPLAY:       gameOptions.botplay = on; break;
        case OPTION_HITSOUND:      gameOptions.hitSound = on; break;
        case OPTION_HEALTH_DRAIN_PLAY:    gameOptions.healthDrainPlay = on; break;
        case OPTION_HEALTH_DRAIN_BOTPLAY: gameOptions.healthDrainBotplay = on; break;
        case OPTION_HORIZONTAL_MENU:
            gameOptions.horizontalMenu = on;
            layoutDirty = true;   // lista vertical <-> uma secao por vez
            break;
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
        if (options[i].type != OPTION_VALUE_LABEL && SearchBox_Match(&search, options[i].text)) return true;
    return false;
}

/* Titulo da secao bate com o filtro? Entao a secao inteira aparece. */
static bool Header_TitleMatches(int header) {
    return SearchBox_Match(&search, options[header].text);
}

/* Com filtro, aparecem as secoes cujo titulo ou alguma opcao bate (mesmo recolhidas),
   e dentro delas: todas as opcoes se o titulo bateu, senao so as que batem.
   Sem filtro: cabecalhos sempre; opcoes somem quando a secao esta recolhida. */
/* Menu horizontal ligado e sem filtro da lupa (com filtro, a lista volta a ser vertical). */
bool Option_HorizontalMode(void) {
    return gameOptions.horizontalMenu && !SearchBox_HasFilter(&search);
}

/* Secao da opcao selecionada = aba aberta (o titulo selecionado tambem abre a sua). */
static bool Option_ComputeVisible(int i) {
    const bool filtering = SearchBox_HasFilter(&search);
    if (Option_HorizontalMode()) {
        if (options[i].type == OPTION_VALUE_HEADER) return false;   // titulos ficam na barra
        return options[i].header == options[currentOption].header;
    }
    if (options[i].type == OPTION_VALUE_HEADER)
        return !filtering || Header_TitleMatches(i) || Section_HasMatch(i);
    if (options[i].type == OPTION_VALUE_LABEL)  // texto estatico: some com a lupa aberta
        return !filtering && !options[options[i].header].collapsed;
    if (filtering)
        return Header_TitleMatches(options[i].header) || SearchBox_Match(&search, options[i].text);
    return !options[options[i].header].collapsed;
}

/* Cache do layout: so recalcula quando o filtro muda ou uma secao abre/fecha (layoutDirty). */
bool layoutDirty = true;
static bool visibleCache[OPTION_COUNT];
static int visualRowCache[OPTION_COUNT]; // linhas visiveis antes de cada opcao

static void Layout_Refresh(void) {
    // Modo horizontal: o layout depende da aba aberta, entao refaz quando ela muda.
    static int layoutTab = -1;
    const int tab = Option_HorizontalMode() ? options[currentOption].header : -1;
    if (!layoutDirty && tab == layoutTab) return;
    layoutTab = tab;

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

/* Cabecalhos sao selecionaveis (para abrir/recolher); textos estaticos nunca; opcoes, so se visiveis e ativas. */
bool Option_IsSelectable(int i) {
    if (options[i].type == OPTION_VALUE_LABEL) return false;
    if (options[i].type == OPTION_VALUE_HEADER) return true;
    return Option_IsVisible(i) && options[i].active;
}

/* Posicao da opcao na tela, em linhas (ignora as recolhidas). */
int Option_VisualIndex(int index) {
    Layout_Refresh();
    return visualRowCache[index];
}

/* Menu horizontal: Left/Right no titulo troca de aba (so a aba aberta aparece). */
void Tab_Step(int dir) {
    for (int i = options[currentOption].header + dir; i >= 0 && i < OPTION_COUNT; i += dir) {
        if (options[i].type == OPTION_VALUE_HEADER) {
            currentOption = i;
            return;
        }
    }
}

/* Menu horizontal: Down do titulo entra na aba; Up/Down dentro dela; Up no 1o item volta ao titulo. */
static void MoveSelectionTabbed(int dir) {
    const int cur = currentOption;

    if (options[cur].type == OPTION_VALUE_HEADER) {
        if (dir > 0)
            for (int i = cur + 1; i < OPTION_COUNT && options[i].type != OPTION_VALUE_HEADER; i++)
                if (Option_IsSelectable(i)) { currentOption = i; return; }
        return;
    }

    const int head = options[cur].header;
    if (dir > 0) {
        for (int i = cur + 1; i < OPTION_COUNT && options[i].type != OPTION_VALUE_HEADER; i++)
            if (Option_IsSelectable(i)) { currentOption = i; return; }
    } else {
        for (int i = cur - 1; i > head; i--)
            if (Option_IsSelectable(i)) { currentOption = i; return; }
        currentOption = head;
    }
}

/* Anda para a proxima linha selecionavel. */
void MoveSelection(int dir) {
    if (Option_HorizontalMode()) {
        MoveSelectionTabbed(dir);
        return;
    }
    for (int i = 0; i < OPTION_COUNT; i++) {
        currentOption = (currentOption + dir + OPTION_COUNT) % OPTION_COUNT;
        if (Option_IsSelectable(currentOption)) break;
    }
}

/* Reseta tudo para o padrao (ENTER duas vezes no reset): aplica no jogo e recarrega a lista. */
void Options_ResetToDefault(void) {
    GameData_ResetOptions();
    GameData_Flush();

    RayGame_SetVsync(gameOptions.vsync);
    if (!gameOptions.vsync) RayGame_SetFPS(gameOptions.fps);
    Jobs_SetThreads(gameOptions.threadLimit);

    Options_LoadValues();

    // Mesmas regras do VSYNC: com V-SYNC ligado, o framerate fica travado.
    options[OPTION_UNCAPPED_FPS].active = !gameOptions.vsync;
    options[OPTION_FPS].active = !gameOptions.vsync && !options[OPTION_UNCAPPED_FPS].value;
    options[OPTION_SHOW_FPS].active = 1;
    for (int i = OPTION_FPS; i <= OPTION_SHOW_FPS; i++) options[i].dirty = true;

    MenuMusic_Play(MenuMusic_SavedIndex(), 1);
    layoutDirty = true;
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
    options[OPTION_LOSE_ICON].value = gameOptions.loseIcon;
    options[OPTION_STRUM_BG].value = gameOptions.strumBgPercent / 10;
    options[OPTION_STRUM_MOBILE].value = gameOptions.strumMobileVisible;
    options[OPTION_HIDDEN_NOTES].value = gameOptions.hiddenNotesAlt;
    options[OPTION_VSYNC].value = gameOptions.vsync;
    // Mesma regra do VSYNC ligado: Uncapped FPS e Framerate ficam inativos (vermelhos) ao entrar na cena.
    options[OPTION_UNCAPPED_FPS].active = !gameOptions.vsync;
    options[OPTION_FPS].active = !gameOptions.vsync && !options[OPTION_UNCAPPED_FPS].value;
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
    options[OPTION_STREAM_JSON].value = gameOptions.streamingJsonParse;
    options[OPTION_BOTPLAY].value = gameOptions.botplay;
    options[OPTION_HORIZONTAL_MENU].value = gameOptions.horizontalMenu;

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
