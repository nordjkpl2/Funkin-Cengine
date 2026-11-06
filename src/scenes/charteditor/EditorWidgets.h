/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: caixas de texto/numero, botoes, listas e bloqueio do mouse. */
#pragma once
#ifndef EDITOR_WIDGETS_H
#define EDITOR_WIDGETS_H

#include "std.h"
#include "Render.h"
#include "scenes/Song.h"

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
char EdButton(Rectangle r, const char* label, int fs);
extern TxtBox* dropBox;
extern int dropCount;
char EdDropBtn(Rectangle r, char open);
extern int dropScroll;
void EdTimeFmt(char* out, size_t n, double ms);
char EdDlgButton(Rectangle r, const char* label, Color edge);
void NumBox_Commit(void);
char EdMenuItem(Rectangle r, const char* label);

#endif
