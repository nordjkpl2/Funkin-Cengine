/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuInternal.h - estado e funcoes compartilhadas pelos modulos do menu de modding
 * (ModdingMenu.c, ModdingMenuInput.c, ModdingMenuDraw.c e hudeditor/HudEditor.c).
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
    MOD_HUD,
    MOD_STUB
} ModdingMode;

#define TAB_COUNT 9 /* numero de entradas em tabs[] (ModdingMenu.c) */
#define TAB_DIALOGUE 6
#define TAB_HUD 7
#define TAB_IMPORT 8

/* Importer: origem do mod -> assets/mods/<nome>. */
#define MODDING_PATH_CAP 512
#define MODDING_MSG_CAP  384

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
void Modding_LoadFx(void);   /* shader do fundo da lista (Create) */
void Modding_UnloadFx(void); /* (Destroy) */
int Modding_TabHit(int i, Vector2 m); /* 1 se o ponto m (coords do mundo) esta sobre a aba i */
int Modding_OpenHit(Vector2 m); /* 1 se o ponto m esta sobre o botao OPEN do painel */
/* Formas usam o retangulo branco do atlas da fonte: texto e formas no mesmo draw call.
   Font com texture.id 0 volta a textura padrao de formas (chamar assim no fim do Draw). */
void Modding_BatchWith(Font font);

#endif
