/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * OptionsMenuInternal.h - tipos, constantes e funcoes compartilhadas pelos modulos do menu de opcoes
 * (OptionsMenu.c, OptionsMenuValues.c, OptionsMenuScreens.c, OptionsMenuIcons.c).
 */
#pragma once
#ifndef OPTIONSMENUINTERNAL_H
#define OPTIONSMENUINTERNAL_H

#include "raylib.h"
#include "threads/cpu/Jobs.h"
#include "ui/SearchBox.h"
#include <stdbool.h>
#include <stddef.h>

#define SCREEN_H 720.0f

// texto e header ta tudo aqui
#define SCROLL_LERP_SPEED 09.0f
#define WHEEL_ROWS_PER_NOTCH 1 // linhas puladas a cada clique da roda do mouse
#define ROW_FONT_SIZE 59
#define LABEL_FONT_SIZE 40 // subtitulos estaticos (ex.: Score, Zoom), menores que as opcoes
#define ROW_PADDING 12
#define HEADER_FONT_SIZE 77
#define HEADER_PADDING 5
#define TEXT_SHADOW 6
#define TEXT_SPACING 1.6f
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
#define STRETCH_HALF 20.0f     // meia largura do icone de Stretch (cantos)
#define TEXT_ICON_HALF 14.0f   // meia altura da caixa com o "T"
#define BOP_ICON_R 7.0f        // raio de cada bolinha do Bopping
#define KEYMON_BOX 15.0f        // lado de cada tecla no icone do Keys Monitor
#define KEYMON_BOX_GAP 4.0f     // espaco entre as teclas
#define KEYBOARD_W 54.0f        // icone de teclado (Scheme)
#define KEYBOARD_H 36.0f
#define ICON_PANEL_W 132.0f     // painel preto atras das notas (Strumline Background)
#define ICON_PANEL_H 40.0f
#define ICON_PANEL_ALPHA 170    // mesma ideia da faixa preta do gameplay
#define HIDDEN_ICON_SCALE 0.16f // tamanho da nota no icone de Hidden Notes
#define HIDDEN_ICON_PERIOD 3.0f // segundos para cada nota subir e sumir
#define MOUSE_ICON_W 22.0f     // mouse do Beat Zoom (UI / Game)
#define MOUSE_ICON_H 34.0f
#define MOUSE_ICON_STROKE 4.0f
#define MONITOR_ICON_W 40.0f    // monitor do V-SYNC (mesma coluna do icone de TV)
#define MONITOR_ICON_H 26.0f
#define TREE_NODE_R 4.0f        // icone de arvore (Threads)
#define TREE_STROKE 3.0f
#define FACE_ICON_R 16.0f       // rosto triste (Losing Icon)
#define ROBOT_ICON_W 30.0f      // robo (Botplay)
#define ROBOT_ICON_H 22.0f
// Centro vertical dos icones de linha (um pouco abaixo do centro da caixa, para ficar na altura do texto).
#define ROW_ICON_DY 3.0f

// Nota musical ao lado de "[Musica]".
#define NOTE_HEAD_RADIUS 13.0f
#define NOTE_STEM_W 7.0f
#define NOTE_STEM_H 56.0f
#define NOTE_FLAG_W 16.0f
#define NOTE_FLAG_H 22.0f

// Engrenagem ao lado de "[Render]".
#define GEAR_HOLE_R 9.0f     // raio interno do anel do corpo
#define GEAR_BODY_R 19.0f    // raio externo do anel do corpo
#define GEAR_TOOTH_LEN 11.0f // dente: comprimento (radial)
#define GEAR_TOOTH_W 10.0f   // dente: largura
#define GEAR_TEETH 8
#define GEAR_OUTER_R (GEAR_BODY_R + GEAR_TOOTH_LEN) // raio total (para alinhar ao texto)

// Aba Musica: Auto passa para a proxima faixa quando a atual termina (so com o menu aberto).
#define MUSIC_SLOTS 65 // 64 faixas + Auto

/* Lupa (so o icone; o campo abre ao clicar). */
#define SEARCH_X 940.0f
#define SEARCH_Y 20.0f
#define SEARCH_W 320.0f
#define SEARCH_H 44.0f

#define THREADS_MAX JOBS_MAX_THREADS

// Menu horizontal: titulos numa barra no topo; so a secao selecionada aparece embaixo.
#define TAB_BAR_Y 100.0f
#define TAB_LOGO_LIFT 92.0f      // altura do icone acima do titulo (a partir da linha do texto)
#define TAB_FONT_SIZE 50
#define TAB_GAP 34.0f            // espaco em volta de cada separador "|"
#define TAB_BAND_H 180.0f        // faixa escura atras da barra (a lista rola por baixo dela)
#define TAB_X 50.0f

// Paleta unica do menu de opcoes (titulos, selecao, dicas).
#define OPT_ACCENT ((Color) {253, 249, 0, 255})          // amarelo principal
#define OPT_ACCENT_TEXT ((Color) {245, 220, 120, 255})   // texto de dica
#define OPT_ACCENT_DIM ((Color) {253, 249, 0, 150})      // borda suave
#define OPT_PANEL ((Color) {0, 0, 0, 170})               // painel escuro atras de textos
#define OPT_PANEL_SEL ((Color) {255, 255, 255, 30})      // fundo da linha selecionada

typedef enum {
    OPTION_VALUE_INT,
    OPTION_VALUE_FLOAT,
    OPTION_VALUE_BOOL,
    OPTION_VALUE_LIST,
    OPTION_VALUE_HEADER,
    OPTION_VALUE_LABEL, // texto estatico: so mostra o text, nao seleciona nem tem valor
    OPTION_VALUE_ACTION // acao: ENTER executa (sem valor)
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
    OPTION_HEADER_RENDER,
    OPTION_SHADERS,
    OPTION_STREAM_JSON,
    OPTION_HEADER_GAMEPLAY,
    OPTION_KEYBOARD_SCHEME,
    OPTION_SCROLL_STYLE,
    OPTION_GHOST,
    OPTION_HITSOUND,
    OPTION_HEALTH_DRAIN_PLAY,
    OPTION_HEADER_VISUAL,
    OPTION_SCORE_LABEL,
    OPTION_SHOW_SCORE,
    OPTION_SCORE_POSITION,
    OPTION_TIME_POSITION,
    OPTION_TIME_STYLE,
    OPTION_ICONS_LABEL,
    OPTION_BOPPING_ICONS,
    OPTION_LOSE_ICON,
    OPTION_ZOOM_LABEL,
    OPTION_ZOOMFACTOR_UI,
    OPTION_ZOOMFACTOR_GAME,
    OPTION_NOTES_LABEL,
    OPTION_NOTE_SPLASH,
    OPTION_STRUM_BG,
    OPTION_STRUM_MOBILE,
    OPTION_HIDDEN_NOTES,
    OPTION_NOTE_COLORS,
    OPTION_HEADER_OTHER,
    OPTION_FILL_SCREEN,
    OPTION_BOTPLAY,
    OPTION_HITSOUND_BOTPLAY,
    OPTION_HEALTH_DRAIN_BOTPLAY,
    OPTION_GAMEPLAY_EXTRA,
    OPTION_HORIZONTAL_MENU,
    OPTION_WARNS,
    OPTION_HEADER_MUSIC,
    OPTION_MENU_MUSIC,
    OPTION_RESET_CONFIG,
    OPTION_COUNT
} OptionOrder;

/* Estado compartilhado. Definido em OptionsMenuValues.c (opcoes) ou OptionsMenu.c (cena). */
extern Option options[OPTION_COUNT];
extern int currentOption;
extern int autoTrack;
extern int limitedThreads;
extern bool layoutDirty;
extern bool bakeDirty;     /* texturas cacheadas (lista, abas, dica) precisam ser refeitas */
extern SearchBox search;
extern bool capturing;
extern bool threadsScreen;
extern bool noteColorScreen;
extern float scrollOffset;
extern float windowLogoX;
extern float otherLogoX;
extern float gameplayLogoX;
extern float visualLogoX;
extern float musicLogoX;
extern float renderLogoX;

/* OptionsMenuValues.c */
int AutoThreads(void);
void Options_LoadValues(void);
void Option_RebuildText(Option* o);
bool MusicAutoOn(void);
void RefreshAutoLabel(void);
void ApplyNumValue(void);
void ApplyBoolValue(void);
bool Option_IsVisible(int i);
bool Option_IsSelectable(int i);
int Option_VisualIndex(int index);
void MoveSelection(int dir);
bool Option_HorizontalMode(void);
void Tab_Step(int dir);
void Options_ResetToDefault(void);

/* OptionsMenuScreens.c */
void Options_KeyName(int key, char* out, size_t size); /* nome curto da tecla (A, SPACE, LEFT...) */
void Capture_Begin(void);
void Capture_Update(void);
void Capture_Draw(void);
void ThreadsScreen_Begin(void);
void ThreadsScreen_Update(void);
void ThreadsScreen_Draw(void);
/* OptionsMenuWarns.c */
extern bool warnsScreen;   /* tela de avisos aberta */
void WarnsScreen_Begin(void);
void WarnsScreen_Update(void);
void WarnsScreen_Draw(void);
/* OptionsMenuNoteColors.c */
void NoteColorScreen_Begin(void);
void NoteColorScreen_Update(void);
void NoteColorScreen_Draw(void);
void NoteColorScreen_Destroy(void);
void NoteColors_DrawLanePreview(int lane, float cx, float cy, float scale);  /* nota + receptor reais da skin */
void NoteColors_ReleaseFx(void);
void NoteColors_DrawNoteIcon(int lane, float cx, float cy, float scale);   /* so a nota (com a cor da coluna) */
/* OptionsMenuGameplayExtra.c: tela Gameplay Extra (Co-Op e Play as) */
extern bool gameplayExtraScreen;
void GameplayExtra_Begin(void);
void GameplayExtra_Update(void);
void GameplayExtra_Draw(void);

/* OptionsMenuIcons.c */
void DrawOptionText(const char* text, float x, float y, int fontSize, Color fill, Color shadow);
void DrawWindowLogo(float rowY, Color fill, Color shadow);
void DrawOtherLogo(float rowY, Color fill, Color shadow);
void DrawGameplayLogo(float rowY, Color fill, Color shadow);
void DrawVisualLogo(float rowY, Color fill, Color shadow);
void DrawMusicLogo(float rowY, Color fill, Color shadow);
void DrawRenderLogo(float rowY, Color fill, Color shadow);
void DrawChevron(float rowY, bool collapsed, Color fill, Color shadow);
void DrawTvIcon(float rowY, Color fill, Color shadow);
void DrawHealthIcons(float rowY, Color fill, Color shadow);
void DrawKeyArrow(float rowY, Color fill, Color shadow);
void DrawExtraIcon(int i, float rowY, Color fill, Color shadow);
void DrawNotesLabelIcon(float x, float cy); /* icone de notas ao lado do rotulo "Notes" */
void DrawResetIcon(float cx, float cy, Color color);

#endif
