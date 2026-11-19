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
#include "widgets-and-menus-ui/SearchBox.h"
#include <stdbool.h>

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

/* Lupa (so o icone; o campo abre ao clicar). */
#define SEARCH_X 940.0f
#define SEARCH_Y 20.0f
#define SEARCH_W 320.0f
#define SEARCH_H 44.0f

#define THREADS_MAX JOBS_MAX_THREADS

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

/* Estado compartilhado. Definido em OptionsMenuValues.c (opcoes) ou OptionsMenu.c (cena). */
extern Option options[OPTION_COUNT];
extern int currentOption;
extern int autoTrack;
extern int limitedThreads;
extern bool layoutDirty;
extern SearchBox search;
extern bool capturing;
extern bool threadsScreen;
extern float scrollOffset;
extern float windowLogoX;
extern float otherLogoX;
extern float gameplayLogoX;
extern float visualLogoX;
extern float musicLogoX;

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

/* OptionsMenuScreens.c */
void Capture_Begin(void);
void Capture_Update(void);
void Capture_Draw(void);
void ThreadsScreen_Begin(void);
void ThreadsScreen_Update(void);
void ThreadsScreen_Draw(void);

/* OptionsMenuIcons.c */
void DrawOptionText(const char* text, float x, float y, int fontSize, Color fill, Color shadow);
void DrawWindowLogo(float rowY, Color fill, Color shadow);
void DrawOtherLogo(float rowY, Color fill, Color shadow);
void DrawGameplayLogo(float rowY, Color fill, Color shadow);
void DrawVisualLogo(float rowY, Color fill, Color shadow);
void DrawMusicLogo(float rowY, Color fill, Color shadow);
void DrawChevron(float rowY, bool collapsed, Color fill, Color shadow);
void DrawTvIcon(float rowY, Color fill, Color shadow);
void DrawHealthIcons(float rowY, Color fill, Color shadow);
void DrawKeyArrow(float rowY, Color fill, Color shadow);
void DrawExtraIcon(int i, float rowY, Color fill, Color shadow);

#endif
