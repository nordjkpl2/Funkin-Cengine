/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuInternal.h - estado e funcoes compartilhadas pelos modulos do menu de modding
 * (ModdingMenu.c, ModdingMenuInput.c, ModdingMenuDraw.c).
 */
#pragma once
#ifndef MODDINGMENUINTERNAL_H
#define MODDINGMENUINTERNAL_H

#include "raylib.h"
#include "Render.h"
#include "data/modimport/ModImport.h"

typedef enum {
    MOD_LIST = 0,
    MOD_IMPORT,
    MOD_STUB
} ModdingMode;

#define TAB_COUNT 3 /* numero de entradas em tabs[] (ModdingMenu.c) */

/* Importer: origem do mod -> assets/mods/<nome>. */
#define MODDING_PATH_CAP 512
#define MODDING_MSG_CAP  256

/* Estado (definido em ModdingMenu.c). */
extern const char *const tabs[TAB_COUNT];
extern ModdingMode mode;
extern int selected;
extern float offset;
extern const char *stubTitle;
extern RayGraphicObject bg;
extern Camera2D cam;

extern char impPath[MODDING_PATH_CAP];
extern char impMsg[MODDING_MSG_CAP];
extern int impMods;
extern int impSongs;
extern long long impKB;
extern char impConfirmDel;
extern ModImportStats impLast;
extern int impLastDup;
extern char impLastValid;
extern ModImportProgress impProg;

extern Color outlineColor;
extern Color fontColor;
extern const Color selectedColor;

/* ModdingMenuInput.c */
void Imp_RefreshUsage(void);
void Imp_PollUsage(void);
void Imp_UpdateInput(void);
void Imp_RunImport(void);

/* ModdingMenuDraw.c */
void Modding_DrawBackground(void);
void Modding_DrawImport(void);
void Modding_DrawStub(void);
void Modding_DrawList(void);

#endif
