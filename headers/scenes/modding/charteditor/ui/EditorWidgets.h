/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: caixas de texto/numero, botoes, listas e bloqueio do mouse. */
#pragma once
#ifndef EDITOR_WIDGETS_H
#define EDITOR_WIDGETS_H

#include "std.h"
#include "Render.h"
#include "data/song/Song.h"

// ---------- paleta (mesma familia do seletor de musicas: violeta escuro + rosa) ----------
#define ED_C_FIELD   CLITERAL(Color){16, 14, 32, 235}    /* fundo de caixa de texto/numero */
#define ED_C_BTN     CLITERAL(Color){30, 26, 54, 240}
#define ED_C_BTN_HOV CLITERAL(Color){56, 46, 96, 255}
#define ED_C_POPUP   CLITERAL(Color){12, 10, 26, 245}    /* menus, listas e janelas por cima */
#define ED_C_BAR     CLITERAL(Color){10, 8, 20, 240}     /* barras de menu e de status */
#define ED_C_EDGE    CLITERAL(Color){120, 100, 200, 150}
#define ED_C_ACCENT  CLITERAL(Color){255, 92, 176, 255}  /* foco, ativo, selecionado */
#define ED_C_DIM     CLITERAL(Color){170, 164, 205, 255} /* texto secundario */
#define ED_C_HINT    CLITERAL(Color){255, 214, 120, 255} /* dicas e avisos leves */
#define ED_C_OK      CLITERAL(Color){120, 230, 150, 255}
#define ED_C_BAD     CLITERAL(Color){255, 100, 110, 255}

// ---------- caixinha de texto (clica e digita) ----------
typedef struct {
    char buf[128];
} TxtBox;

// caixinha de numero (digita e ENTER confirma)
typedef struct {
    char buf[16];
} NumBox;

// qual lista suspensa esta aberta
enum { DROP_NONE, DROP_BOYFRIEND, DROP_OPPONENT, DROP_GIRLFRIEND, DROP_STAGE, DROP_SKIN };

typedef enum { DLG_NONE, DLG_OPEN, DLG_SAVEAS, DLG_LEAVE } CeDlg;

// lista por cima (clica e preenche a caixinha). Rola com a roda do mouse.
#define DROP_ROWS_MAX 14

extern TxtBox* focusBox;
extern NumBox* focusNum;
extern NumBox nbSecBpm;
extern NumBox nbTopBpm;
extern NumBox nbSpeed;
extern NumBox nbLen;
void NumBox_Refresh(NumBox* nb, float v, char isInt);
void TxtBox_Update(TxtBox* box, Rectangle r, char blocked);
void TxtBox_Draw(TxtBox* box, Rectangle r, const char* hint);
void NumBox_Update(NumBox* nb, Rectangle r, char blocked);
void NumBox_Draw(NumBox* nb, Rectangle r);
extern char activeDrop;
extern char dropJustOpened;
extern float dropX, dropY, dropW;
extern CeDlg ceDlg;
extern int menuOpen;
extern Camera2D camEd;
Vector2 EdMouseWorld(void);
char EdHover(Rectangle r);       /* mouse (mundo) dentro de r */
char EdUiBlocked(void);          /* janela, lista ou menu aberto: os paineis nao recebem clique */
char EdHelpOpen(void);           /* overlay de atalhos (TAB) aberto: grade e paineis nao recebem clique */
void EdHelpToggle(void);
void Ed_DrawBox(Rectangle r, Color fill, Color edge); /* caixa arredondada com contorno */
char EdButton(Rectangle r, const char* label, int fs);
extern TxtBox* dropBox;
extern char (*dropNames)[SONG_NAME_MAX];
extern int dropCount;
char EdDropBtn(Rectangle r, char open);
extern int dropScroll;
void EdTimeFmt(char* out, size_t n, double ms);
char EdDlgButton(Rectangle r, const char* label, Color edge);
void NumBox_Commit(void);
/* icones desenhados com primitivas (sem textura): so nos itens de menu que pedem */
typedef enum { ED_ICON_NONE = 0, ED_ICON_PLAYHEAD, ED_ICON_RESTART, ED_ICON_TRASH, ED_ICON_SAVE, ED_ICON_DOOR, ED_ICON_BACK } EdIcon;
char EdMenuItem(Rectangle r, const char* label);
char EdMenuItemIcon(Rectangle r, const char* label, EdIcon icon);
// texto com sombra (legivel sobre fundo da mesma cor)
void Ed_DrawText(Font f, const char* t, Vector2 p, float size, float spacing, Color c);
/* Titulo de painel com barra de destaque colorida a esquerda. */
void Ed_DrawHeader(float x, float y, const char* t, Color c);

#endif
